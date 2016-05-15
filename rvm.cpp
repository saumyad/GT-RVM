#include "rvm.h"
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <map>
#include <list>
#include <errno.h>
#include <fstream>
#include <unistd.h>
#include <algorithm>

using namespace std;

std::map<trans_t, trans_t_int*> transaction_map;
std::map<region, std::list<log_entry*> > log_map;
std::map<std::string, region> seg_map;
std::map<std::string, rvm_t> rvm_list_map;
std::map<rvm_t, std::list<region> > segment_names;
std::map<trans_t_int*, std::list<region> > segment_names_trans;
std::map<void**, std::string> loc_seg_map;


std::string get_backing_store(rvm_t rvm, const char * segname){
	string segpath(rvm->dir_path);
	segpath = segpath + "/" + static_cast<string>(segname);
	return segpath;
}

/*
  Initialize the library with the specified directory as backing store.
*/
rvm_t rvm_init(const char *directory){
	rvm_t r = (rvm_t) malloc(sizeof(RVM_T));
	r->dir_path = new char[strlen(directory)+1];
	strcpy(r->dir_path, directory); /* copy name into the new var */
	std::string dirname(directory);
	// check if the same-name rvm already exists
	map<std::string, rvm_t>::iterator it = rvm_list_map.find(dirname.c_str());

	if(it != rvm_list_map.end()) {
		//throw some error and terminate
		cout << "[RVM-Error] RVM with the same name already exists. Aborting now." << std::endl;
		exit(-1);
	}

	int dirfd = mkdir(directory, 0777);
	if (dirfd != 0){
	std::cout << "[RVM-Error] Error creating the directory " << directory <<". Checking the cause...";
	if (errno == 17){
		printf("Directory already exists\n");
	}
	else if (errno == 12){
			printf("Out of memory\n");
		}
		else{
			printf("Unknown error\n");
		}
	}
	else{
		cout << "Directory created\n";
	}
	r->log_path = new char[LOG_PATH_SIZE + strlen(directory) + 1];
	strcpy(r->log_path, directory);
	strcat(r->log_path, "/");
	strcat(r->log_path, LOG_FILE_NAME);

	fstream fs;
	fs.open(r->log_path, ios::out);
	fs.close();

	rvm_list_map[dirname.c_str()] = r;
	cout << "init done" << endl;	
	return r;
}

/*
  map a segment from disk into memory. If the segment does not already exist, 
  then create it and give it size size_to_create. If the segment exists but is 
  shorter than size_to_create, then extend it until it is long enough. It is an 
  error to try to map the same segment twice.
*/
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create){
	cout << "In map. " << endl;	
	map<rvm_t, list<region> >::iterator ita = segment_names.find(rvm);
	if(ita != segment_names.end()) {
		list<region> l1 = ( list<region> ) ita->second;
		for(list<region>::iterator i = l1.begin(); i != l1.end(); ++i) {
			region r1 = *i;
			if(r1 != NULL && r1->seg_name == segname) {
				cout << "[RVM-Error] Segment in the rvm already mapped. Aborting.." << std::endl;
				return (void *) -1;
			}
		}
	}
	else {
		cout << "RVM not mapped yet. " << endl;
	}
	region reg;
	map<string, region>::iterator ireg = seg_map.find(segname);
	if(ireg == seg_map.end()) {
		reg = (region) malloc(sizeof(reg_struct));
		reg->start_address_disk = 0;
		reg->seg_name = (char * ) segname;
		reg->seg_size = size_to_create;
			
		char * buffer;
		buffer = (char *) malloc(sizeof(char) * size_to_create);
		if (buffer == NULL){
			cout << "Could not get enough memory to create the segment buffer. Aborting\n";
			return (void *) -1;
		}
		reg->orig_content = buffer;
		char * buffer1;
		buffer1 = (char *) malloc(sizeof(char) * size_to_create);
		if (buffer1 == NULL){
			cout << "Could not get enough memory to create the segment buffer. Aborting\n";
			return (void *) -1;
		}
		reg->start_addr_inmem = buffer1;

		reg->is_modified = false;
		reg->is_in_rvm = true;
		reg->is_in_trans = false;

		std::string sname(segname);
		string segpath = get_backing_store(rvm, segname);
		
		// Find the backing store
		if( access(segpath.c_str(), F_OK ) != -1 ) {
			/*
			ifstream f;
			f.open(segpath.c_str(), ios::binary);
			cout << "Segment backing file already exists, checking for segment size." << std::endl;
			f.seekg(0, ios::end); 
			streampos origsize = f.tellg();
			int size = static_cast<int>(origsize);
			cout << size << size_to_create << endl;
			*/
			FILE* f;
			f = fopen(segpath.c_str(), "r+");
			fseek(f, 0L, SEEK_END);
			int sz = ftell(f);
			rewind(f);
			//streampos origsize = infile.tellg(); 
			int size = static_cast<int>(sz);
			
			if(size < size_to_create) {
				off_t s = static_cast<off_t>(size_to_create);
				cout << "Increasing segment size from " << size << " to " << size_to_create << std::endl;
				if (truncate(segpath.c_str(), s) != 0) {
					cout << "[RVM-Error] Error increasing size of backing file." << std::endl;
				}
			}
			//infile.close();
		}
		else {
			cout << "Segment backing file does not exist..Creating" << endl;
			std::fstream f_seg(segpath.c_str(),  fstream::out | fstream::in | fstream::trunc);
			if (!f_seg.good()) {
				std::cout << "[RVM-Error] Segment backing file could not be created, Aborting." << std::endl;
				return (void *) -1;
			}
			//create file of size size_to_create
			f_seg.seekp(size_to_create);
			f_seg.write("", 1);
			f_seg.close();
		}
		
		// Map the segment to memory
		FILE* pFile = fopen (segpath.c_str() , "rb" );
		if (pFile==NULL) {
			cout<< "[RVM-Error] Segment Backing File cannot be opened. Mapping Failed. " <<std::endl;
			return (void *) -1;
		}

		size_t result = fread(reg->orig_content,1,size_to_create,pFile);
		if (result != size_to_create) {
			cout<< "[RVM-Error] Segment Backing File size reading error. Read " << result << " compared to " << reg->seg_size << " bytes" <<std::endl;
			return (void *) -1;
		}
		fclose (pFile);
	}
	else {
		reg = (region)ireg->second;
		//write back now?
	}
	memcpy((void*)reg->start_addr_inmem,(void*)reg->orig_content, size_to_create);
	// Add the segment to global segment map.
	seg_map[segname] = reg;

	// Add the segment into the rvm segment list.
	list<region> rg;
	list<region> l1 = segment_names[rvm];
	if(!l1.empty()) {
		rg = l1;
	}
	rg.push_back(reg); 
	segment_names[rvm] = rg;
	loc_seg_map[(void**)reg->start_addr_inmem] = reg->seg_name;
	cout << "Finished map." << endl;
	return (void**)(reg->start_addr_inmem);
}
/*
  unmap a segment from memory.
*/

void rvm_unmap(rvm_t rvm, void *segbase) {
	cout << "In unmap" << endl;
	map<void**, string>::iterator imap = loc_seg_map.find((void**)segbase);
	if(imap == loc_seg_map.end()) {
		cout << "Segment not mapped. Aborting now." << endl;
		return;
	}
	string segname = (string)imap->second;
	
	map<rvm_t,list<region> >::iterator it2 = segment_names.find(rvm);
	if(it2 == segment_names.end()) {
		//throw some error and terminate
		cout << "[RVM-Error] The rvm has no mapped segments. Cannot unmap " << segname <<endl;
		return;
	}
	else {
		list<region> lreg = (list<region> ) it2->second;
		if (seg_map.find(segname) == seg_map.end()){
			cout << "[RVM-Error] Segment not found in the global map. Aborting" << endl;
			return;
		}
		// find and unmap the segment, Note that segment memory still exists.
		region targetreg = seg_map[segname];
		if(!targetreg) {
			cout << "Invalid segment. Aborting now." << endl;
			return;
		}
		//Check if region is even mapped to this rvm
		bool found = false;
		for(list<region>::iterator i = lreg.begin(); i != lreg.end(); ++i) {
			region r = *i;
			if(r==targetreg) {
				found = true;
				break;
			}
		}
		if ( found  == false) {
			cout << "Segment not mapped to current RVM. Aborting now." << endl;
			return;
		}
		lreg.remove(targetreg);
		targetreg->is_in_rvm = false;
		if(lreg.size() == 0) {
			segment_names.erase(rvm);
			rvm_list_map.erase(rvm->dir_path);
		}
		else {
			segment_names[rvm] = lreg;
		}
		loc_seg_map.erase((void**)targetreg->start_addr_inmem);
	}
	cout << "Segment unmapped successfully.." << endl;
	return;
}

/*
  destroy a segment completely, erasing its backing store. This function
   should not be called on a segment that is currently mapped.
 */
void rvm_destroy(rvm_t rvm, const char *segname) {
	//Dont unmap, check if it already mapped and return an error
	cout << "In destroy" << endl;
	string seg((char*)segname);
	if (seg_map.find(seg) == seg_map.end()){
		cout << "[RVM-Error] Segment not found in the global map. Aborting" << endl;
		return;
	}	
	region reg = seg_map[seg];

	if (reg->is_in_rvm == true){
		cout << "[RVM-Fatal] Segment already mapped to a rvm. Unmap and try again" << endl;
		exit(-1);
	}
	
	//delete backing store
	string segpath = get_backing_store(rvm, segname);
	if(remove(segpath.c_str()) != 0) {
		cout << "[RVM-Error] Segment file deletion failed. Probably file " << segpath << " not found." <<endl;
		return;
	}
	
	//Free the segment memory
	seg_map.erase(seg);
	free(reg);
	cout << "Segment successfully destroyed." <<endl;
	return;
}

/*
  begin a transaction that will modify the segments listed in segbases. 
  If any of the specified segments is already being modified by a 
  transaction, then the call should fail and return (trans_t) -1. 
  Note that trant_t needs to be able to be typecasted to an integer type.
 */
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
	cout << "In rvm_begin_trans." << endl;
	if(transaction_map.empty()) {
		//TODO: anything here?
	}
	trans_t_int* trans = (trans_t_int*) malloc(sizeof(trans_t_int));
	//rvm.num_trans++;
	trans->rvm = rvm;
	int tid = transaction_map.size();
	trans->trans_id = (trans_t) (tid+1);
	for(int i = 0; i<numsegs; i++) {
		map<void**, string>::iterator imap = loc_seg_map.find((void**)segbases[i]);
		if(imap == loc_seg_map.end()) {
			cout << "Segment not mapped. Aborting now." << endl;
			return (trans_t) -1;
		}
		string segname = (string)imap->second;
		//string segname((char*)segbases[i]);
		map<rvm_t, list<region> >::iterator it_rvm = segment_names.find(rvm);
		if(it_rvm == segment_names.end()) {
			//throw some error and terminate
			cout << "Invalid RVM or no mapped segments. Aborting now." << endl;
			return (trans_t) -1;
		}
		else {
			//check if segment is mapped to this rvm
			
			region reg = seg_map[segname];
			if(!reg) {
				cout << "Invalid segment. Aborting now." << endl;
				return (trans_t) -1;
			}
			list<region> l1 = (list<region> ) it_rvm->second;
			bool found = false;
			for(list<region>::iterator i = l1.begin(); i!= l1.end(); ++i) {
				region r = *i;
				if(r == reg) {
					found = true;
					break;
				}
			}
			if(found == false) {
				cout << "Segment not mapped to the current RVM. Aborting now." << endl;
				return (trans_t) -1;
			}
			else {
				//region reg = (region)it1->second;
				if (reg->is_modified || reg->is_in_trans) {
					cout << "Segment already being modified or is marked for modification by another transaction. Aborting now.";
					return (trans_t) -1;
				}
				else {
					map<trans_t_int*, list<region> >::iterator it2 = segment_names_trans.find(trans);
					list<region> r1;
					if(it2 != segment_names_trans.end()) {
						list<region> l1 = (list<region> )it2->second;
						for(list<region>::iterator i2 = l1.begin(); i2 != l1.end(); ++i2) {
							region thisreg = (region) *i2;
							if(thisreg == reg) {
								std::cout << "Element already existed" << '\n';
								continue;
							}
						} 
						r1 = l1;
					}
					r1.push_back(reg);
					segment_names_trans[trans] = r1;
				}
			}
		}
	}
	transaction_map[trans->trans_id] = trans;
	cout << "Finished rvm_begin_trans." << endl;
	return (trans_t) trans->trans_id;
}

/*
  declare that the library is about to modify a specified range of 
  memory in the specified segment. The segment must be one of the 
  segments specified in the call to rvm_begin_trans. Your library 
  needs to ensure that the old memory has been saved, in case an 
  abort is executed. It is legal call rvm_about_to_modify multiple 
  times on the same memory area.
*/
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
	cout << "In rvm_about_to_modify" << endl;
	map<trans_t, trans_t_int*>::iterator i = transaction_map.find(tid);
	if(i == transaction_map.end()) {
		//throw some error and terminate
		cout << "Transaction ID not found. Aborting now." << endl;
		return;
	}
	else {
		trans_t_int* trans = (trans_t_int*) i->second;
		map<void**, string>::iterator imap = loc_seg_map.find((void**)segbase);
		if(imap == loc_seg_map.end()) {
			cout << "Segment not mapped. Aborting now." << endl;
			return;
		}
		string segname = (string)imap->second;
		//string segname((char*) segbase);
		region reg = seg_map[segname];
		map<trans_t_int*, list<region> >::iterator it1 = segment_names_trans.find(trans);
		if(it1 == segment_names_trans.end()) {
			//throw some error and terminate
			cout << "No segments mapped for this transaction. Aborting now." << endl;
			return;
		}
		else {
			reg->is_modified = true;
			if(log_map.empty()) {
				//TODO: anything here?
			}
			log_entry* l = (log_entry*) malloc(sizeof(log_entry));
			l->trans_id = trans->trans_id;
			l->reg = reg;
			size_t regsize = static_cast<size_t>(size);
			//l->contents = (char*) malloc(size);
			l->contents = (char*)(reg->start_addr_inmem) + offset;
			l->offset = offset;
			l->size = size;
			//memcpy(l->contents, (char*)(reg->start_addr_inmem) + offset, size);
			list<log_entry*> list_l;
			list<log_entry*> l1 = log_map[reg];
			if(!l1.empty()) {
				list_l = l1;
			}
			list_l.push_back(l);
			log_map[reg] = list_l;
		}
	}
	return;
}

/*
commit all changes that have been made within the specified transaction. 
When the call returns, then enough information should have been saved to disk 
so that, even if the program crashes, the changes will be seen by the program 
when it restarts.
*/
void rvm_commit_trans(trans_t tid){
	trans_t_int* trans = transaction_map[tid];
	int trans_id = static_cast<int>(tid);
	if(trans == NULL) {
		cout << "Transaction ID not found. Aborting now." << endl;
		return;
	}
	else {
		//Iterate over all the segment logs and write to redo log on disk.
		rvm_t rvm = trans->rvm;
		//Open redo log file for append
		string logpath(rvm->log_path);
		FILE *f;
		f = fopen(logpath.c_str(), "a");
		int logwrite;
		if (f == NULL) {
			printf("Couldn't open file with error %d\n", errno);
			fflush(stdout);
			return;
		}
		map<region, list<log_entry*> >::iterator it_log;
		map<trans_t_int*, list<region> >::iterator itblah = segment_names_trans.find(trans);
		if(itblah == segment_names_trans.end()) {
			cout << "Transaction not found. Aborting." << endl;
			return;
		}
		else {
			list<region> l1 = (list<region> ) itblah->second;
			for(list<region>::iterator itl = l1.begin(); itl != l1.end(); ++itl) {
				region reg = *itl;
				string segname = reg->seg_name;
				char* orig = (char*)reg->orig_content;
				//Find list of log entries for each region and append.
				it_log = log_map.find(reg);
				int offset = 0;
				int size = static_cast<int>(reg->seg_size);
				if(it_log == log_map.end()) {
					cout << "No logs found for current region. Moving on now.";
				}
				else {
					list<log_entry*> list_l = (list<log_entry*> ) it_log->second;
					for(list<log_entry*>::iterator i = list_l.begin(); i != list_l.end(); ++i) {
						log_entry* le = *i;
						//trans_id, seg_name, start_addr, size, content
						char* content = (char*) le->contents;
						int offset = le->offset;
						int size = strlen(content);//le->size;
						//update reg->orig_content
						memcpy((void*)((char*)reg->orig_content + offset), (void*)content, size);
						logwrite = fprintf(f, "%d,%s,%d,%d,%s\n", trans_id, segname.c_str(), offset, size, content);
						
						FILE *f_seg;
				
						string segfilepath = get_backing_store(rvm, segname.c_str());
						f_seg = fopen(segfilepath.c_str(), "r+");
						int ret = fseek(f_seg, offset, SEEK_SET);
						if (ret != 0) {
							printf("Couldn't seek in segfile with error %d\n", errno);
							fflush(stdout);
						}
						ret = fprintf(f_seg, "%s", content);
						//fwrite(((void*)reg->start_addr_inmem), sizeof(char), size, f_seg);
						if (ret != size) {
							printf("Writing out filed with error %d\n", errno);
							fflush(stdout);
						}
						ret = fclose(f_seg);
						
						if (ret != 0) {
							printf("Couldn't close segfile ith error %d\n", errno);
							fflush(stdout);
						}
					}
				}
				//logwrite = fprintf(f, "%d,%s,%d,%d,%s\n", trans_id, segname.c_str(), offset, size, (char*)reg->orig_content);
				//logwrite = fprintf(f, "%d,%s,%d,%d,%s\n", trans_id, segname.c_str(), offset, size, (char*)reg->start_addr_inmem);
				reg->is_modified = false;
				reg->is_in_trans = false;
			}
			segment_names_trans.erase(trans);
		}
		//rvm->num_trans--;
		fclose(f);		
	}
	return;
}

/*
  undo all changes that have happened within the specified transaction.
 */
 
void rvm_abort_trans(trans_t tid){
	trans_t_int* trans = transaction_map[tid];
	if(trans == NULL) {
		//throw some error and terminate
		cout << "Transaction ID not found. Aborting now." << endl;
		return;
	}
	else {
		//Iterate over all the segment logs and write to redo log on disk.
		//rvm_t* rvm = trans.rvm;
		map<region, list<log_entry*> >::iterator it_log;

		map<trans_t_int*, list<region> >::iterator itblah = segment_names_trans.find(trans);
		if(itblah == segment_names_trans.end()) {
			cout << "Transaction not found. Aborting." << endl;
			return;
		}
		else {
			list<region> l1 = (list<region> ) itblah->second;
			for(list<region>::iterator itl = l1.begin(); itl != l1.end(); ++itl) {
				region reg = *itl;
				string segname = reg->seg_name;
				size_t ssize = reg->seg_size;
				int size = static_cast<int>(ssize);
				memcpy(reg->start_addr_inmem, reg->orig_content, size);
				reg->is_modified = false;
				reg->is_in_trans = false;
				//Find list of log entries for each region and reset.
				it_log = log_map.find(reg);
				if(it_log == log_map.end()) {
					cout << "No mapped segments found for current region. Moving on now.";
				}
				else {
					log_map.erase(reg);
				}
			}
			//rvm->num_trans--;
		}
	}
	return;
}

/*
 play through any committed or aborted items in the log file(s) 
 and shrink the log file(s) as much as possible.
*/
void rvm_truncate_log(rvm_t rvm) {
	//Read the log and combine all the data for each segment.
	string logpath(rvm->log_path);
	string dirpath(rvm->dir_path);
	FILE *f_log;
	FILE *f_seg;
	//f_log = fopen(logpath.c_str(), "r");

	ifstream infile(logpath.c_str());
	//logwrite = fprintf(f, "%d,%s,%d,%d,%s\n", trans_id, segname.c_str(), offset, size, content);
	int trans_id, offset, size; 
	char* segname;
	char* content;
	char c1,c2,c3,c4; 
	while ((infile >> trans_id >> c1 >> segname >> c2 >> offset >> c3 >> size >> c4 >>content ) && (c1 == ',') && (c2 == ',') && (c3 == ',') && (c4 == ',')) {
		//get segment file
		//write to that file
		//close
		string segfilepath = dirpath+"/"+segname;
		f_seg = fopen(segfilepath.c_str(), "r+");

		region reg = seg_map[segname];
		if(reg == NULL) {
			cout << "Segment is not mapped. Aborting now.";
			return;
		}
		else {
			int ret = fseek(f_seg, offset, SEEK_SET);
			if (ret != 0) {
				printf("Couldn't seek in segfile with error %d\n", errno);
				fflush(stdout);
			}
			ret = fprintf(f_seg, "%s", content);
			//fwrite((void*) content, sizeof(char), size, f_seg);
			if (ret != size) {
				printf("Writing out filed with error %d\n", errno);
				fflush(stdout);
			}
			ret = fclose(f_seg);
			if (ret != 0) {
				printf("Couldn't close segfile ith error %d\n", errno);
				fflush(stdout);
			}
		}
	}
	//fclose(f_log); 
	f_log = fopen(logpath.c_str(), "w");
	fclose(f_log);
}