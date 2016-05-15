#ifndef __LIBRVM_INTERNAL__
#define __LIBRVM_INTERNAL__

#include <iostream>
#include <cstring>
#include <queue>
#include <stdbool.h>
#include <map>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <fstream>
#include <unistd.h>
#include <algorithm>


typedef int trans_t; 

typedef struct reg_struct{
	int start_address_disk;
	char* seg_name;	//use as an identifier?
	size_t seg_size;
	bool is_modified;
	bool is_in_trans;
	bool is_in_rvm;
	void* start_addr_inmem;
	void* orig_content;
	/*
	reg_struct(const reg_struct& other){
	    start_address_disk = other.start_address_disk;
	    seg_name = (char*) malloc( std::strlen((char* ) other.seg_name) + 1); 
		std::strcpy(seg_name, other.seg_name);
		seg_size = other.seg_size;
		is_modified = other.is_modified;
		is_in_rvm = other.is_in_rvm;
		is_in_trans = other.is_in_trans;
		start_addr_inmem = malloc( std::strlen((char*)other.start_addr_inmem) + 1); 
		memcpy(start_addr_inmem, other.start_addr_inmem, std::strlen((char*)other.start_addr_inmem) );
		//std::strcpy(start_addr_inmem, other.start_addr_inmem);
		orig_content = malloc( std::strlen((char*)other.orig_content) + 1); 
		memcpy(orig_content, other.orig_content, std::strlen((char*)other.orig_content) );
		//std::strcpy(orig_content, other.orig_content);
	}

	reg_struct operator= (const reg_struct& other){
		if (this != &other) {
			start_address_disk = other.start_address_disk;
			seg_name = (char*) malloc( std::strlen((char* ) other.seg_name) + 1); 
			strcpy(seg_name, other.seg_name);
			seg_size = other.seg_size;
			is_modified = other.is_modified;
			is_in_rvm = other.is_in_rvm;
			is_in_trans = other.is_in_trans;
			start_addr_inmem = malloc( std::strlen((char*)other.start_addr_inmem) + 1); 
			memcpy(start_addr_inmem, other.start_addr_inmem, std::strlen((char*)other.start_addr_inmem) );
			//std::strcpy(start_addr_inmem, other.start_addr_inmem);
			orig_content = malloc( std::strlen((char*)other.orig_content) + 1); 
			memcpy(orig_content, other.orig_content, std::strlen((char*)other.orig_content) );
			//std::strcpy(orig_content, other.orig_content);
		}
		return *this;
	}
	*/
} region_struct;

typedef region_struct *region;


typedef struct RVM_t{
	char* dir_path;
	char* log_path;
	std::map<region, std::string> mapped_segments;
	/*
	RVM_t(const RVM_t& other){
	    dir_path = (char*) malloc( std::strlen((char* ) other.dir_path) + 1); 
		std::strcpy(dir_path, other.dir_path);
		log_path = (char*) malloc( std::strlen((char* ) other.log_path) + 1); 
		std::strcpy(log_path, other.log_path);
	}

	RVM_t operator= (const RVM_t& other){
		if (this != &other) {
			dir_path = (char*) malloc( std::strlen((char* ) other.dir_path) + 1); 
			std::strcpy(dir_path, other.dir_path);
			log_path = (char*) malloc( std::strlen((char* ) other.log_path) + 1); 
			std::strcpy(log_path, other.log_path);
		}
		return *this;
	}
	*/
 } RVM_T;
typedef RVM_T *rvm_t;


typedef struct trans_t_1{
	trans_t trans_id;
	int num_seg;
	rvm_t rvm;
	std::map<region, std::string> mapped_segments;
	/*
	trans_t_1(const trans_t_1& other){
	    trans_id = other.trans_id; 
	    num_seg = other.num_seg; 
		rvm = other.rvm; 
	}

	trans_t_1 operator= (const trans_t_1& other){
		if (this != &other) {
			trans_id = other.trans_id; 
		    num_seg = other.num_seg; 
			rvm = other.rvm;
		}
		return *this;
	}
	*/
} trans_t_int;

typedef struct log_e{
	trans_t trans_id;
	region reg;
	int offset;
	int size;
	void* contents;
	/*
	log_e(const log_e& other){
	    trans_id = other.trans_id; 
	    reg = other.reg; 
		offset = other.offset; 
		size = other.size; 
		contents = malloc( std::strlen((char*)other.contents) + 1); 
		memcpy(contents, other.contents, std::strlen((char*)other.contents) );
	}

	log_e operator= (const log_e& other){
		if (this != &other) {
			trans_id = other.trans_id; 
		    reg = other.reg; 
			offset = other.offset; 
			size = other.size; 
			contents = malloc( std::strlen((char*)other.contents) + 1); 
			memcpy(contents, other.contents, std::strlen((char*)other.contents) );
		}
		return *this;
	}
	*/

} log_entry;

#define LOG_FILE_NAME "redolog.txt"
#define DIR_PATH_SIZE 256
#define LOG_PATH_SIZE (DIR_PATH_SIZE+12)


extern std::map<trans_t, trans_t_int*> transaction_map;
extern std::map<region, std::list<log_entry*> > log_map;
extern std::map<std::string, region> seg_map;
extern std::map<std::string, rvm_t> rvm_list_map;
extern std::map<rvm_t, std::list<region> > segment_names;
extern std::map<trans_t_int*, std::list<region> > segment_names_trans;
extern std::map<void**, std::string > loc_seg_map;

#endif
