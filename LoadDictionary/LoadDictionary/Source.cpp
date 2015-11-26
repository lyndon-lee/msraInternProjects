#include <stdio.h>
#include <tchar.h>
#include <io.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <WinSock2.h>
#include <Windows.h>
#include <conio.h>
#pragma comment(lib, "ws2_32.lib")

// Cplusplus Library
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <strstream>
#include <fstream>
#include <exception>
using namespace std;

// Boost Library
#include <boost\filesystem.hpp>
#include <boost\algorithm\string.hpp>

// OpenCV library
#include <opencv2\core\core.hpp>
#include <opencv2\highgui\highgui.hpp>
#define LINE_MAX_LEN 65536
const int featlen = 1118;


typedef struct category{
	int categoryid;
	string categoryname;
	map<string, cv::Mat> photoprob;
	vector<pair<string, cv::Mat>> wordvec;
} category;
const string indexroot = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\index\\";
const string imagefeatroot1K = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\imageTagging1K\\";
const string imagefeatroot118 = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\imageTagging118\\";
const string imagefeatroot1118 = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\imageTagging1118\\";
const string suggestionroot = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\suggestion\\";
const string albumroot = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\album\\";
const string categoryroot = "\\\\msravrt-02\\d$\\Users\\v-doyu\\TaggingData\\category\\";

const string dictfile = "D:\\Data\\Word2Vec\\GoogleNews-vectors-negative300.bin";
void loadDictionary(string dictfile, unordered_map<string, cv::Mat> &wordDB){
	ifstream wordStream(dictfile, ios::binary);
	if (!wordStream.is_open()){
		cerr << "the dictionary file can not be opened!\t" << dictfile << endl;
		exit(1);
	}

	// read number of words and vector size
	long long wordnum, vecsize;
	wordStream >> wordnum >> vecsize;

	// load vocabulary and vectors
	for (long long wordid = 0; wordid < wordnum; wordid++){
		string wordname;
		wordStream >> wordname;
		wordStream.get();
		cv::Mat vec(1, vecsize, CV_32FC1, 0.0);
		wordStream.read((char *)vec.ptr<float>(0), sizeof(float)*vecsize);
		double vec_norm = cv::norm(vec, cv::NORM_L2);
		vec = vec / vec_norm;
		wordDB[wordname] = vec;
	}

	cout << "dictionay size: " << wordDB.size() << endl;
	cout << "load dictionary success!" << endl;
	return;
}
void loadDictionaryFast(string dictfile, unordered_map<string, cv::Mat> &wordDB){
	// Create file object
	HANDLE hFile = CreateFileA(dictfile.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE){
		cerr << "error: fail to create file object:" << GetLastError() << endl;
		exit(1);
	}

	// Create file mapping object
	HANDLE hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hFileMap == NULL){
		cerr << "error: fail to creat file mapping object:" << GetLastError() << endl;
		exit(1);
	}

	//// Get file size
	//DWORD dwFileSizeHigh;
	//__int64 qwFileSize = GetFileSize(hFile, &dwFileSizeHigh);
	//qwFileSize = qwFileSize | (((__int64)dwFileSizeHigh) << 32);

	LPVOID pvFile = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
	char *p = (char *)pvFile;

	// read number of words and vector size
	long long wordnum = atoll(p);
	p = strpbrk(p, " \t\n") + 1;
	long long vecsize = atoll(p);
	p = strpbrk(p, " \t\n") + 1;

	// load vocabulary and vectors
	for (long long wordid = 0; wordid < wordnum; wordid++){
		char wordname[LINE_MAX_LEN];
		char *q = strpbrk(p, " \t\n");
		int len = q - p;
		memcpy_s(wordname, LINE_MAX_LEN, p, len);
		wordname[len] = '\0';
		q++;

		cv::Mat vec(1, vecsize, CV_32FC1, 0.0);
		memcpy_s(vec.ptr<float>(0), sizeof(float)*vecsize, q, sizeof(float)*vecsize);
		p = q + sizeof(float)*vecsize;

		double vec_norm = cv::norm(vec, cv::NORM_L2);
		vec = vec / vec_norm;
		wordDB[wordname] = vec;
	}
	cout << "dictionay size: " << wordDB.size() << endl;
	cout << "load dictionary success!" << endl;

	//// Get system allocation granularity
	//SYSTEM_INFO SysInfo;
	//GetSystemInfo(&SysInfo);
	//DWORD dwGran = SysInfo.dwAllocationGranularity;

	//// Define local variable
	//__int64 qwFileOffset = 0;
	//DWORD dwBlockBytes = 1000 * dwGran;

	//while (qwFileSize > 0){
	//	// Load data block
	//	DWORD dwFileOffsetLow = (DWORD)(qwFileOffset & 0xffffffff);
	//	DWORD dwFileOffsetHigh = (DWORD)(qwFileOffset >> 32);
	//	if (qwFileSize < dwBlockBytes){
	//		pvFile = MapViewOfFile(hFileMap, FILE_MAP_READ, dwFileOffsetHigh, dwFileOffsetLow, qwFileSize);
	//		qwFileOffset += qwFileSize;
	//		qwFileSize = 0;
	//	}
	//	else{
	//		pvFile = MapViewOfFile(hFileMap, FILE_MAP_READ, dwFileOffsetHigh, dwFileOffsetLow, dwBlockBytes);
	//		qwFileOffset += dwBlockBytes;
	//		qwFileSize -= dwBlockBytes;
	//	}
	//	// read data

	//}

	UnmapViewOfFile(pvFile);
	CloseHandle(hFileMap);
	CloseHandle(hFile);
}
void loadCategoriesFromFile(vector<category> &categories, string categoriesfile){
	cv::FileStorage fs(categoriesfile, cv::FileStorage::READ);
	int categorynum;
	fs["categoryCount"] >> categorynum;
	// load each category
	cv::FileNode categorylist = fs["categories"];
	for (cv::FileNodeIterator category_it = categorylist.begin(); category_it != categorylist.end(); category_it++){
		category categoryobj;
		categoryobj.categoryname = (string)(*category_it)["categoryname"];
		categoryobj.categoryid = (int)(*category_it)["categoryid"];
		cv::FileNode objnode = (cv::FileNode)(*category_it)["obj2vec"];

		// split the categoryname into object list
		vector<string> objlist;
		boost::split(objlist, categoryobj.categoryname, boost::is_any_of(",\n"), boost::token_compress_on);
		for (vector<string>::iterator objlist_it = objlist.begin(); objlist_it != objlist.end(); objlist_it++){
			string objname = *objlist_it;
			boost::trim(objname);
			// replace non-alphanumeric character in objname with '_'
			string objname_transform;
			for (int i = 0; i < objname.size(); i++){
				if (isalnum(objname[i]) || objname[i] == ' ' || objname[i] == '_' || objname[i] == '-')
					objname_transform += objname[i];
				else
					objname_transform += '_';
			}
			cv::Mat vec;
			(*category_it)["obj2vec"][objname_transform] >> vec;
			if (vec.data)
				categoryobj.wordvec.push_back(make_pair(objname, vec));
		}
		categories.push_back(categoryobj);
	}
	fs.release();
	cout << "category size: " << categories.size() << endl;
	cout << "load categories success!" << endl;
}
void loadCategories(string namelist, unordered_map<string, cv::Mat> &wordDB, vector<category> &categories){
	assert(wordDB.size() > 0);
	unordered_map<string, cv::Mat>::iterator wordDB_it = wordDB.begin();
	int veclen = wordDB_it->second.cols;

	ifstream nameStream(namelist);
	char buff[LINE_MAX_LEN];
	int idx = 0;
	while (nameStream.getline(buff, LINE_MAX_LEN)){
		category categoryobj;
		// load category id and name
		categoryobj.categoryid = idx;
		categoryobj.categoryname = buff;
		// load each object name and its vector representation
		vector<string> objlist;
		boost::split(objlist, buff, boost::is_any_of(",\n"), boost::token_compress_on);

		////// for category118
		string objname = *objlist.rbegin();
		boost::trim(objname);
		// convert the objname to a vector representation
		cv::Mat vec = cv::Mat::zeros(1, veclen, CV_32FC1);
		string objname_convert = boost::replace_all_copy(objname, " ", "_");
		if (wordDB.find(objname_convert) != wordDB.end()){
			vec += wordDB[objname_convert];
		}
		else{
			vector<string> wordlist;
			boost::split(wordlist, objname, boost::is_any_of(" "), boost::token_compress_on);
			for (vector<string>::iterator wordlist_it = wordlist.begin(); wordlist_it != wordlist.end(); wordlist_it++){
				if (wordDB.find(*wordlist_it) != wordDB.end()){
					vec += wordDB[*wordlist_it];
				}
				else{
					cerr << "warning: can not find the object name in vocabulary:" << *wordlist_it << endl;
				}
			}
		}
		double vec_norm = cv::norm(vec, cv::NORM_L2);
		if (vec_norm > 0)
			vec = vec / vec_norm;
		categoryobj.wordvec.push_back(make_pair(objname, vec));
		//////

		////// for category1K
		//for (vector<string>::iterator objlist_it = objlist.begin(); objlist_it != objlist.end(); objlist_it++){
		//	string objname = *objlist_it;
		//	boost::trim(objname);
		//	// convert the objname to a vector representation
		//	cv::Mat vec = cv::Mat::zeros(1, veclen, CV_32FC1);
		//	string objname_convert = boost::replace_all_copy(objname, " ", "_");
		//	if (wordDB.find(objname_convert) != wordDB.end()){
		//		vec += wordDB[objname_convert];
		//	}
		//	else{
		//		vector<string> wordlist;
		//		boost::split(wordlist, objname, boost::is_any_of(" "), boost::token_compress_on);
		//		for (vector<string>::iterator wordlist_it = wordlist.begin(); wordlist_it != wordlist.end(); wordlist_it++){
		//			if (wordDB.find(*wordlist_it) != wordDB.end()){
		//				vec += wordDB[*wordlist_it];
		//			}
		//			else{
		//				cerr << "warning: can not find the object name in vocabulary:" << *wordlist_it << endl;
		//			}
		//		}
		//	}
		//	double vec_norm = cv::norm(vec, cv::NORM_L2);
		//	if (vec_norm)
		//		vec = vec / vec_norm;
		//	categoryobj.wordvec.push_back(make_pair(objname, vec));
		//}
		//////
		idx++;
		categories.push_back(categoryobj);
	}
	cout << "load categories success!" << endl;
	return;
}
void loadCategories1k(string namelist, unordered_map<string, cv::Mat> &wordDB, vector<category> &categories){
	assert(wordDB.size() > 0);
	unordered_map<string, cv::Mat>::iterator wordDB_it = wordDB.begin();
	int veclen = wordDB_it->second.cols;

	ifstream nameStream(namelist);
	char buff[LINE_MAX_LEN];
	int idx = 0;
	while (nameStream.getline(buff, LINE_MAX_LEN)){
		category categoryobj;
		// load category id and name
		categoryobj.categoryid = idx;
		categoryobj.categoryname = buff;
		// load each object name and its vector representation
		vector<string> objlist;
		boost::split(objlist, buff, boost::is_any_of(",\n"), boost::token_compress_on);

		////// for category118
		//string objname = *objlist.rbegin();
		//boost::trim(objname);
		//// convert the objname to a vector representation
		//cv::Mat vec = cv::Mat::zeros(1, veclen, CV_32FC1);
		//string objname_convert = boost::replace_all_copy(objname, " ", "_");
		//if (wordDB.find(objname_convert) != wordDB.end()){
		//	vec += wordDB[objname_convert];
		//}
		//else{
		//	vector<string> wordlist;
		//	boost::split(wordlist, objname, boost::is_any_of(" "), boost::token_compress_on);
		//	for (vector<string>::iterator wordlist_it = wordlist.begin(); wordlist_it != wordlist.end(); wordlist_it++){
		//		if (wordDB.find(*wordlist_it) != wordDB.end()){
		//			vec += wordDB[*wordlist_it];
		//		}
		//		else{
		//			cerr << "warning: can not find the object name in vocabulary:" << *wordlist_it << endl;
		//		}
		//	}
		//}
		//double vec_norm = cv::norm(vec, cv::NORM_L2);
		//if (vec_norm > 0)
		//	vec = vec / vec_norm;
		//categoryobj.wordvec.push_back(make_pair(objname, vec));
		//////

		////// for category1K
		for (vector<string>::iterator objlist_it = objlist.begin(); objlist_it != objlist.end(); objlist_it++){
			string objname = *objlist_it;
			boost::trim(objname);
			// convert the objname to a vector representation
			cv::Mat vec = cv::Mat::zeros(1, veclen, CV_32FC1);
			string objname_convert = boost::replace_all_copy(objname, " ", "_");
			if (wordDB.find(objname_convert) != wordDB.end()){
				vec += wordDB[objname_convert];
			}
			else{
				vector<string> wordlist;
				boost::split(wordlist, objname, boost::is_any_of(" "), boost::token_compress_on);
				for (vector<string>::iterator wordlist_it = wordlist.begin(); wordlist_it != wordlist.end(); wordlist_it++){
					if (wordDB.find(*wordlist_it) != wordDB.end()){
						vec += wordDB[*wordlist_it];
					}
					else{
						cerr << "warning: can not find the object name in vocabulary:" << *wordlist_it << endl;
					}
				}
			}
			double vec_norm = cv::norm(vec, cv::NORM_L2);
			if (vec_norm)
				vec = vec / vec_norm;
			categoryobj.wordvec.push_back(make_pair(objname, vec));
		}
		//////
		idx++;
		categories.push_back(categoryobj);
	}
	cout << "load categories success!" << endl;
	return;
}

void saveCategoriesToFile(vector<category> &categories, string categoriesfile){
	assert(categories.size() > 0);
	cv::FileStorage fs(categoriesfile, cv::FileStorage::WRITE);
	int categorynum = (int)categories.size();
	int categoryid = 0;
	fs << "categoryCount" << categorynum;
	fs << "categories" << "[";
	for (vector<category>::iterator categories_it = categories.begin(); categories_it != categories.end(); categories_it++){
		fs << "{";
		fs << "categoryname" << categories_it->categoryname;
		fs << "categoryid" << categories_it->categoryid;
		vector<pair<string, cv::Mat>> wordvec = categories_it->wordvec;
		fs << "obj2vec" << "{";
		for (vector<pair<string, cv::Mat>>::iterator wordvec_it = wordvec.begin(); wordvec_it != wordvec.end(); wordvec_it++){
			string objname = wordvec_it->first;
			cv::Mat vec = wordvec_it->second;
			// replace non-alphanumeric character in objname with '_'
			string objname_transform;
			for (int i = 0; i < objname.size(); i++){
				if (isalnum(objname[i]) || objname[i] == ' ' || objname[i] == '_' || objname[i] == '-')
					objname_transform += objname[i];
				else
					objname_transform += '_';
			}
			fs << objname_transform << vec;
		}
		fs << "}";
		fs << "}";
	}
	fs << "]";
	fs.release();
}
int main(){
	unordered_map<string, cv::Mat> wordDB;
	loadDictionaryFast(dictfile, wordDB);

	string namelist1K ="namelist.txt";
	string namelist118 ="namelist_v2.txt";
	vector<category> categories;
	vector<category> categories1000;
	loadCategories(namelist118, wordDB, categories);
	loadCategories1k(namelist1K, wordDB, categories1000);
	string categoryfile1K = "categories1K.yml";
	string categoryfile118 = "categories118.yml";
	string categoryfile1118 = "categories1118.yml";
	//loadCategoriesFromFile(categories, categoryfile1K);
	//loadCategoriesFromFile(categories, categoryfile118);
	for (int i = 0; i < 118; i++){
		categories[i].categoryid += 1000;
		categories1000.push_back(categories[i]);
	}
	saveCategoriesToFile(categories1000, categoryfile1118);
	cin.get();
	cin.get();
	cin.get();
}