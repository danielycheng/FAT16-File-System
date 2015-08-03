/*Daniel Cheng - dyc8av
Written March 31, 2015
This is where implementations of cd, ls, cpin, cpout, exit
are located. In addition, there are many helper functions and
global variables used to traverse and manipulate the FAT16 
file system. All functions are stored in this single file*/

#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <sstream> 
#include <unistd.h>
#include <math.h> 
using namespace std;

/*struct to access variables within boot block*/
struct Fat16BootSector{
	unsigned char jmp[3];
	char oem[8];
	unsigned short sector_size;
	unsigned char sectors_per_cluster;
	unsigned short reserved_sectors;
	unsigned char number_of_fats;
	unsigned short root_dir_entries;
	unsigned short total_sectors_short;
	unsigned char media_descriptor;
	unsigned short fat_size_sectors;
	unsigned short sectors_per_track;
	unsigned short number_of_heads;
	unsigned long hidden_sectors;
	unsigned long total_sectors_long;
	unsigned char drive_number;
	unsigned char current_head;
	unsigned char boot_signature;
	unsigned long volume_id;
	char volume_label[11];
	char fs_type[8];
	char boot_code[448];
	unsigned short boot_sector_signature;
} __attribute((packed));

/*overlay the boot block with a struct */
union boot {
	char bytes[512];
	Fat16BootSector BPB;
};

/*struct to access variables within a directory entry*/
struct Fat16Entry{
	unsigned char filename[8];
	unsigned char ext[3];
	unsigned char attributes;
	unsigned char reserved[10];
	unsigned short modify_time;
	unsigned short modify_date;
	unsigned short starting_cluster;
	unsigned int file_size;
} __attribute((packed));

/*overlay a directory entry with a struct*/
union filebeg {
	char bytes[32];
	Fat16Entry meta;
};

/*struct to access values within a FAT index*/
struct FAT{
	short index;
} __attribute((packed));

/*overlay a fat index with a struct*/
union FATtable {
	char bytes[2];
	FAT entry;
};

/*global variables*/
string cwd = "/"; //keep track of current working directory
long currentlocation; //keep track of byte offset
int firstCluster; //keep track of position of first cluster
long globalroot; //keep track of position of root
long begFAT1; //keep track of first FAT 
short FATindex; //keep track of FAT index
int clusterSize; //keep track of size of cluster

/*return the byte offset of a given cluster*/
long readCluster(int n, union boot bblock) {
	long position;
	if(n == 0) {
		position = globalroot;
	}
	else {
		position = firstCluster + (bblock.BPB.sectors_per_cluster
			* bblock.BPB.sector_size * (n-2));
	}
	return position;
}

/*convert a string to a char * for all UPPERCASE
used for filename comparison*/
char* convertString(string s, bool flag) {
	char * tempdir = new char[100]; 
	for(int i = 0; i < 100; i++) {
		tempdir[i] = ' ';
	}
	for(int i = 0; i < s.length(); i++) {
		if(s.at(i) == '.' && flag) {
			break;
		}
		tempdir[i] = toupper(s.at(i));
	}
	return tempdir;
}

/*convert a string to a char* for all lowercase*/
char* convertStringLower(string s) {
	char * tempdir1 = new char[100]; 
	for(int i = 0; i < 100; i++) {
		tempdir1[i] = ' ';
	}
	for(int i = 0; i < s.length(); i++) {
		tempdir1[i] = s.at(i);
	}
	return tempdir1;
}

/*track the current working directory to prompt the user*/
void trackcwd(string s) {
	for(int i = 0; i < s.length(); i++) {
		s.at(i) = toupper(s.at(i));
	}
	if(cwd == "/") {
		cwd = cwd + s;
	}
	else if(s == "..") {
		int length = cwd.length()-1;
		while(cwd.at(length) != '/') {
			cwd = cwd.substr(0, cwd.length() - 1);
			length--;
		}
		if(cwd.length() != 1) {
			cwd = cwd.substr(0, cwd.length() - 1);
		}
	}
	else {
		cwd = cwd + "/" + s;
	}
}

/*trim the whitespace around any char **/
char* trim(char* c, int n) {
	char * name = new char[100];
	for(int i = 0; i < n; i++) {
		if(c[i] != ' ') {
			name[i] = c[i];
		}
	}
	return name;
}

/*given an index in the FAT table, will return the value at that index*/
short findFAT(int cluster, FILE *file, union FATtable table, union boot bblock) {
	int temploc = begFAT1;
	int temp = temploc + (cluster * 2);
	fseek(file,temp,SEEK_SET);
	fread(table.bytes,2,1,file);
	return table.entry.index;
}

/*change the current working directory to user input
will return the index of the first cluster in order
to traverse FAT*/
short cd(string s, FILE *file, union filebeg place, union boot bblock, bool flag) {
	if(s == "") {
		cout << "Please provide a <directory>!" << endl;
	}
	/*return to root directory*/
	else if(s == "/") {
		currentlocation = globalroot;
		cwd = "/";
	}
	else {
		int temp = currentlocation;
		int truetemp = temp;
		int testtemp;
		bool exist = false;
	/*check if directory exists in cwd - if relative/absolute path*/
		while(place.meta.filename[0] != 0x00) {
			if(strncmp(convertString(s,false),(char *)place.meta.filename,8) == 0) {
				/*file does exist in cwd*/
				exist = true;
			}
			truetemp = truetemp + 32;
			fseek(file,truetemp,SEEK_SET);
			fread(place.bytes,32,1,file);

		} 
		/*will be relative path and search cwd for filename*/
		if(exist) {
			fseek(file,temp,SEEK_SET);
			fread(place.bytes,32,1,file);
			while(place.meta.filename[0] != 0x00) {
				if(strncmp(convertString(s,false),(char *)place.meta.filename,8) == 0) {
					/*change the currentlocation to new cluster*/
					currentlocation = readCluster(place.meta.starting_cluster, bblock);
					/*update the cwd if "cd" was input*/
					if(flag) {
						trackcwd(s);
					}
					return place.meta.starting_cluster;
				}
				temp = temp + 32;
				fseek(file,temp,SEEK_SET);
				fread(place.bytes,32,1,file);
			}
		}
		/*will be absolute path, reset currentlocation to root*/
		else {
			testtemp = globalroot;
			fseek(file,testtemp,SEEK_SET);
			fread(place.bytes,32,1,file);
			while(place.meta.filename[0] != 0x00) {
				if(strncmp(convertString(s,false),(char *)place.meta.filename,8) == 0) {
					/*change the currentlocation to new cluster*/
					currentlocation = readCluster(place.meta.starting_cluster, bblock);
					/*update the cwd if "cd" was input*/
					if(flag) {
						cwd = "/";
						trackcwd(s);
					}
					return place.meta.starting_cluster;
				}
				testtemp = testtemp + 32;
				fseek(file,testtemp,SEEK_SET);
				fread(place.bytes,32,1,file);
			}
		}
	}
}

/*print out the contents of a directory*/
void ls(FILE *file, union filebeg place) {
	int temp = currentlocation;
	fseek(file,temp,SEEK_SET);
	fread(place.bytes,32,1,file);
	int times = clusterSize/32;
	/*print out files in appropiate format*/
	for(int i = 0; i < times; i++) { //goes through one cluster
		if(place.meta.attributes == 0x08) { //volume label
			printf("Volume Label: %.8s\n",trim((char*)place.meta.filename,8));
		}
		if(place.meta.attributes == 0x10) { //directory
			printf("D  %.8s\n",trim((char*)place.meta.filename,8));
		}
		if(place.meta.attributes == 0x20) { //file
			printf("   %.8s%s%.3s\n",trim((char*)place.meta.filename,8), ".", place.meta.ext);
		}
		if(place.meta.attributes == 0x02) { //hidden file
			printf("H  %.8s%s%.3s\n",trim((char*)place.meta.filename,8), ".", place.meta.ext);
		}
		if(place.meta.attributes == 0x04) { //system file
			printf("S  %.8s%s%.3s\n",trim((char*)place.meta.filename,8), ".", place.meta.ext);
		}
		temp = temp + 32;
		fseek(file,temp,SEEK_SET);
		fread(place.bytes,32,1,file);
	}
}

/*run stuff!!!*/
int main(int argc, char* argv[]) {
	/*check if valid input*/
	if(argv[1] == NULL) {
		cout << "No file provided" << endl;
		exit(0);
	}

	/*read boot block into memory*/
	FILE * file;
	file = fopen(argv[1],"r");
	union boot bblock;
	fread(bblock.bytes,512,1,file);

	/*read first root directory entry into memory*/
	int root = (bblock.BPB.reserved_sectors * 512) 
	+ (bblock.BPB.number_of_fats * bblock.BPB.fat_size_sectors * 512);
	union filebeg name; 
	fseek(file,root,SEEK_SET);
	fread(name.bytes,32,1,file);

	/*set global variables*/
	currentlocation = root;
	globalroot = root;
	firstCluster = root + (bblock.BPB.root_dir_entries * 32);
	begFAT1 = bblock.BPB.reserved_sectors * bblock.BPB.sector_size;
	clusterSize = bblock.BPB.sectors_per_cluster * bblock.BPB.sector_size;
	/*--------- end loading global variables -------*/


	while(true) { //as the program is running
		cout << ": " + cwd + " > "; //prompt user with current working directory
		
		/*begin parsing of input*/
		string input = "";
		string sTokens[100];
		getline(cin,input);
		char* cinput = NULL;

		cinput = new char[input.size() +1];
		strcpy(cinput,input.c_str());
		char * tokens = strtok(cinput, " /"); 
		int count = 0;
		
		union filebeg place; //where dir entries will be loaded
		union FATtable table; //fat table index values

		while(tokens != NULL) { 
			sTokens[count] = tokens;
			count++;
			tokens = strtok(NULL," /");
		}
		/*------- end parsing of input --------*/

		if(input == "exit") {
			/*terminate*/
			exit(0); 
		}

		else if(input == "cd /") {
			/*return cwd to root*/
			cd("/", file, place, bblock, true);
		}

		else if(sTokens[0] == "ls") {
			int temp = currentlocation;
			/*execute ls relative to currentlocation*/
			if(sTokens[1] == "") { 
				/*display file if in root*/
				if(temp == globalroot) {
					ls(file, place);
				}
				else {
					/*display all items by moving through FAT table*/
					short current = FATindex;
					if(current != 0) {
						ls(file, place);
						/*check if FAT is last entry*/
						while(findFAT(current, file, table, bblock) != -1) {
							/*will find value of next entry and ls contents of current block*/
							short tempcurrent = findFAT(current, file, table, bblock);
							currentlocation = readCluster(tempcurrent, bblock);
							ls(file, place);
							current = tempcurrent;
						}
					}
					else {
						cout << "Directory does not exist" << endl;
					}
					/*reset parameters*/
					currentlocation = temp;
				}

			}
			/*execute ls for an absolute path*/
			else {
				/*change cwd to that path to change currentlocation*/
				for(int i = 1; i < count; i++) {
					FATindex = cd(sTokens[i], file, place, bblock, false);
				}
				/*display all items by moving through FAT table*/
				short current = FATindex;
				if(current != 0) {
					ls(file,place);
					/*check if FAT is last entry*/
					while(findFAT(current, file, table, bblock) != -1) {
						/*will find value of next entry and ls contents of current block*/
						short tempcurrent = findFAT(current, file, table, bblock);
						currentlocation = readCluster(tempcurrent, bblock);
						ls(file, place);
						current = tempcurrent;
					}
				}
				else {
					cout << "Directory does not exist" << endl;
				}
				/*reset parameters*/
				currentlocation = temp;
			}
		}

		/*execute cd when prompted*/
		else if(sTokens[0] == "cd") {
			for(int i = 1; i < count; i++) {
				FATindex = cd(sTokens[i], file, place, bblock, true);
			}
		}
		else if(sTokens[0] == "cpin") {
			string src = "";
			string dst = "";
			long size;
			/*parse paths to src and dst*/
			int temp = currentlocation;
			int tempFAT = FATindex;
			int endsrc = 0;
			int startdst = 0;
			long filesizein; //filesize
			for(int i = 1; i < count; i++) {
				if(sTokens[i].find('.') != string::npos) {
					endsrc = i;
					startdst = i+1;
					break;
				}
			}
			for(int i = 1; i < startdst; i++) {
				src = src + sTokens[i] + "/";
			}
			src = src.substr(0, src.length() - 1);
			for(int i = startdst; i < count; i++) {
				dst = dst + "/" + sTokens[i];
			}
			/*---- finish parsing ----*/

			/*open the file given a path*/
			file = fopen(trim(convertStringLower(src),src.length()),"rb");
			if(file == NULL) {
				cout << "Directory does not exist" << endl;
			}
			else {
				/*find the file size*/
				fseek(file,0,SEEK_END);
				filesizein = ftell(file);
				/*create buffer to load data into*/
				char bufin[filesizein];
				/*load data into buffer*/
				fseek(file,0,SEEK_SET);
				fread(bufin,filesizein,1,file);

				/*calculate number of required clusters*/
				int clustreq = 1 + (int)ceil(filesizein/clusterSize);
				/*find number of FAT indicies*/
				int fatSize = ((bblock.BPB.reserved_sectors * 512) 
					+ (bblock.BPB.fat_size_sectors * 512))/2;
				int storeFAT[clustreq]; //store free FATs

				/*find the free clusters using FAT and store their location*/
				for(int j = 0; j < clustreq; j++) {
					for(int i = 0; i < fatSize; i++) {
						if(findFAT(i, file, table, bblock) == 8) {
							if(storeFAT[j] != i) {
								storeFAT[j] = i;
								break;
							}
						}
					}
				}
				
			// Update both FAT in order to link the clusters together, save value of starting cluster
			// Go to <dst> and create directory entry with all information, including starting cluster
			// Copy buffer to disk starting from starting cluster and traversing as dictated by FAT
			}

			/*reset variables*/
			currentlocation = temp;
			FATindex = tempFAT;
			file = fopen(argv[1],"r");
			fseek(file,temp,SEEK_SET);
			fread(place.bytes,32,1,file);
		}


		else if(sTokens[0] == "cpout") {
			char * buf; 
			getcwd(buf,64); //get the current working directory
			int temp = currentlocation;
			int tempFAT = FATindex;
			int doesexist; 
			/*parse input into <src> and <dst>*/
			string src = "";
			string dst = buf;
			int endsrc = 0;
			int startdst = 0;
			for(int i = 1; i < count; i++) {
				if(sTokens[i].find('.') != string::npos) {
					endsrc = i;
					startdst = i+1;
					break;
				}
			}
			for(int i = startdst; i < count; i++) {
				dst = dst + "/" + sTokens[i];
			}
			/*---- end parsing ----*/
			/*change directory to byte offset that wish to read at*/
			for(int i = 1; i < endsrc; i++) {
				doesexist = cd(sTokens[i], file, place, bblock, false);
			}
			int truetemp = currentlocation;
			/*check if exists*/
			if(doesexist == 0) {
				cout << "Directory does not exist" << endl;
			}
			else {
				/*read in the desired directory entry into memory*/
				fseek(file,truetemp,SEEK_SET);
				fread(place.bytes,32,1,file);
				while(place.meta.filename[0] != 0x00) {
					if(strncmp(convertString(sTokens[endsrc],true),(char *)place.meta.filename,8) == 0) {
						break;
					}
					truetemp = truetemp + 32;
					fseek(file,truetemp,SEEK_SET);
					fread(place.bytes,32,1,file);
				}
				/*calculate location of file that is to be copied*/
				int byteoffset = readCluster(place.meta.starting_cluster, bblock);
				int cfilesize = place.meta.file_size;
				/*load into buffer*/
				char cpoutbuf[cfilesize];
				fseek(file,byteoffset,SEEK_SET);
				fread(cpoutbuf,cfilesize,1,file);
				fclose(file);
				/*write the buffer to disk*/
				file = fopen(trim(convertStringLower(dst),dst.length()),"w");
				fwrite(cpoutbuf,cfilesize,1,file);
			}
			/*reset parameters*/
			fclose(file);
			file = fopen(argv[1],"r");
			fseek(file,temp,SEEK_SET);
			fread(place.bytes,32,1,file);
			currentlocation = temp;
			FATindex = tempFAT;
		}
	}
}