//Liam Glockner and Calista Greenway 
//Assignment0 
//ECE 570
// Written in C++

#include <iostream>
#include<map>
#include <set>
#include<vector>
#include <string>
#include <fstream>
#include <utility>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <iterator>
#include <sstream>
#include <algorithm>



using namespace std;
typedef map<string, set<int> > invertedIndex; //define the map 

int docNum = 0; // the current document number

bool isNotAlpha(char c) {
    return isalpha(c) == 0;
}

string removeSpecialCharacter(string s) {
    for (int i = 0; i < s.size(); i++) {
        // Finding the character whose
        // ASCII value fall under this
        // range
        if (s[i] < 'A' || s[i] > 'Z' && s[i] < 'a' || s[i] > 'z') {
            // erase function to erase
            // the character
            s.erase(i, 1);
            i--;
        }
    }
    return s;
}

std::string ltrim(const std::string &s)
{
    const std::string WHITESPACE = " \n\r\t\f\v";
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string rtrim(const std::string &s)
{
    const std::string WHITESPACE = " \n\r\t\f\v";
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

void show(invertedIndex &mapOfSet) {
    for (invertedIndex::iterator it = mapOfSet.begin();
         it != mapOfSet.end();
         it++) {
  
        // Key is integer
        cout << it->first <<':';
  
        // Value is a set of ints
        set<int> st = it->second;
  
        // Strings will be printed
        // in sorted order as set
        // maintains the order
        for (set<int>::iterator it2 = st.begin(); it2 != st.end(); it2++) {
            cout << ' ' << (*it2);
        }
            cout << '\n';
        
    }
}
int main (int argc, char* argv[]) { // create the main function 
    string word; // the current word
    string word1;
    string word2;
    string filename;
    invertedIndex index;
    ifstream inputfile;
    inputfile.open(argv[1]); // open the input file

    if(!inputfile) {
        cout << "input file could not be opened " << endl; //input file not opened and exits 
        exit(1);
    }
    
//============ Reading input.txt for other file names
    while(getline(inputfile,filename)) { // while there is a line in the input file 
        filename = rtrim(filename);
        ifstream fileRead(filename.c_str()); 
        if (!fileRead.is_open()) {
            cout << "internal file could not be opened" << endl;
            exit(1);
        }

        while(getline(fileRead,word,'\n')) {
            std::replace_if(word.begin(),word.end(),isNotAlpha,'-');
            stringstream ss(word.c_str());
            while(getline(ss, word, '-')){ //need to handle case where there is punctuation.
                word2 = " ";
                bool flag = false;
                //=====handling of edge case with number in middle====//
        
                word = ltrim(word.c_str());
                word = rtrim(word.c_str());
                for(int i=0;i<word.length()+1;i++) {
                    if(isdigit(word[i])){
                        flag = true;
                    }
                    if(!isdigit(word[i]) && flag) {
                        word2 += word[i];
                        word[i] = ' ';
                    }
                    if(ispunct(word[i])) {
                        word[i] = ' ';
                    }
                    
                }
                word = removeSpecialCharacter(word.c_str());
                if (word.empty()) {
                    continue;
                }
                index[word].insert(docNum);

                invertedIndex::iterator itr2 = index.find(word2.c_str());
                if(itr2 != index.end()) {
                    continue;
                }
                word2 = ltrim(word2.c_str());
                word2 = rtrim(word2.c_str());
                word2 = removeSpecialCharacter(word2.c_str());
                if(word2.empty()) {
                    continue;
                }
                index[word2].insert(docNum);
                
            
            }
        }
        fileRead.close();
        docNum++;
    
    }
    show(index);
    inputfile.close();
}
    


