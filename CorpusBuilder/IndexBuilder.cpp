#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <cmath>
#include <iterator>


#include "boost/serialization/set.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "unordered_map_serialization.h"
#include "boost\filesystem.hpp"

#include "InvertedIndexStructures.h"



/*
utility method for partitioning match results in decreasing order of score
*/
struct ResultSetComparer{
	bool operator() (const std::pair<int,double>& first, const std::pair<int,double>& second)
	{
		return (first.second > second.second);
	}
};



/*
breaks text into tokens and converts them into lowercase
only english supported now..
*/
class WordBreaker{
	static char* delimiter;
public:

	//tokenize and normalize the english text using delimter[s]
	std::vector<std::string > BreakEnglishText(const std::string& text)
	{
		char* input = (char*)text.c_str();
		std::vector<std::string > tokens;

		char* temp = strtok(input,delimiter);
		while( temp != NULL)
		{

			std::string token(temp);
			std::transform(token.begin(),token.end(),token.begin(),tolower);
			tokens.push_back(token);
			temp = strtok(NULL,delimiter);
		}

		return tokens;
	}
};

char* WordBreaker::delimiter = " ,:;'!@#$%^&*()-_=+~\".";




/*
The main workhorse , it provides 2 main APIs 
1) BuildIndex: Creates an Inverted Index from document
2) ServeIndex: Loads the Index in-memory & performs ranking of matching documents
*/
class IndexBuilder{

	std::string docPath;
	std::auto_ptr<WordBreaker> _wordBreaker;
	std::auto_ptr<InvertedIndex> _indexPtr;
	int _totalDocs;
	static char* delim;

	/* 
	This api takes the tokenized and normalized body of the document and adds it into index.
	*/
	void AddDocToIndex(const int& docId,char* title,const std::vector<std::string>& body)
	{
		DocEntry doc(docId,title,"");
		doc._docLength = (int)body.size();
		boost::unordered_map<std::string,int> word_count;


		for(size_t i=0;i<body.size();++i)
		{
			//Add into the doc index
			boost::unordered_map<std::string,int>::const_iterator itor = word_count.find(body[i]);
			if(itor == word_count.end() ){
				word_count[body[i]] = 1;
			}else{
				word_count[body[i]] = word_count[body[i]] + 1;

				if( word_count[body[i]] > doc._maxFreqWordCount)
					doc._maxFreqWordCount = word_count[body[i]];
			}

			//Add the token to inverted index
			boost::unordered_map<std::string,IndexEntry>::iterator index_iter = _indexPtr->_wordIndex.find( body[i] );
			if( index_iter == _indexPtr->_wordIndex.end() ){
				IndexEntry entry;
				entry._docSet.insert(docId);
				_indexPtr->_wordIndex[ body[i] ] = entry;
			}else{
				index_iter->second._docSet.insert(docId);
			}

		}

		doc._wordCount = word_count;

		_indexPtr->_docMetaData[docId] = doc;


		//Print(doc);

	} 

	void Print(const DocEntry& doc)
	{
		std::cout<<"************************************************"<<std::endl;
		std::cout<<doc._docId<<" : " <<doc._title<<std::endl;
		std::cout<<"max: " <<doc._maxFreqWordCount<<std::endl;
		boost::unordered_map<std::string,int>::const_iterator i = doc._wordCount.begin();
		while( i != doc._wordCount.end()){
			std::cout<<i->first<<" : " <<i->second<<std::endl;
			++i;
		}
		std::cout<<"************************************************"<<std::endl;

	}


public:



	IndexBuilder(const std::string& docFile):docPath(docFile),
		_wordBreaker( new WordBreaker()),
		_indexPtr(new InvertedIndex()),
		_totalDocs(0){}
	~IndexBuilder() {_wordBreaker.reset();_indexPtr.reset();}


	/* 
	This api takes a tsv file - containing 'documents' and generates an in-memory inverted index.
	The index is dumped to disk using the Serialize api.
	*/
	void BuildIndex()
	{
		std::ifstream inFile(docPath.c_str(),std::ifstream::in);
		if(!inFile.good()){
			std::cerr<<"Unable to read file: " <<docPath<<std::endl;
			exit(-1);
		}

		inFile.sync_with_stdio(false);
		std::string line;
		int docId = 0;

		while( getline(inFile,line))
		{
			if( docId % 10000 == 0)
				std::cerr<<"At document: " <<docId<<std::endl;

			std::vector<char* > tempVector;
			char* parts = strtok( (char*)line.c_str() , delim);
			while( parts != NULL)
			{
				tempVector.push_back(parts);
				parts = strtok(NULL,delim);
			}

			assert(tempVector.size() == 3);

			std::vector<std::string>& tokens = _wordBreaker->BreakEnglishText(tempVector[2]);

			AddDocToIndex(docId,tempVector[1],tokens);
			++docId;
		}
		_totalDocs = docId;
	}


	/*
	This api takes the user input - and looks up the word[s] in the inverted index.
	The candidate documents are then ranked based on tf-idf BOW (bag of words) model
	*/
	std::vector<std::pair<int,double> > ServeIndex(const std::string& word,int topK)
	{
		//tokenize and normalize the user text
		std::vector<std::string>& word_tokens = _wordBreaker->BreakEnglishText(word.c_str());

		std::vector<std::pair<int,double> > results;

		//generate the candidate document set
		std::set<int> candSet;
		bool foundAny = false;
		for(size_t i=0;i<word_tokens.size();++i)
		{
			boost::unordered_map<std::string,IndexEntry>::iterator itor = _indexPtr->_wordIndex.find(word_tokens[i]);

			if( itor == _indexPtr->_wordIndex.end() )
				continue;

			else{
				//first entry which was found
				if(!foundAny){
					candSet = itor->second._docSet;
					foundAny = true;
				} else{
					std::set<int> temp;
					set_intersection(candSet.begin(),candSet.end(),(itor->second)._docSet.begin(),(itor->second)._docSet.end(),inserter(temp,temp.begin()));
					candSet.clear();
					candSet = temp;
				}
			}

		}

		return Rank(word_tokens,candSet,topK);
	}


	/*
	This api implements the tf-idf BOW model for computing {token} -> {document} similarity.
	In a multi word query , the score is computed for each individual token and added up to get final score.
	More details can be found on: http://en.wikipedia.org/wiki/Tf%E2%80%93idf
	*/
	std::vector<std::pair<int,double> > Rank(const std::vector<std::string>& word_tokens,const std::set<int>& docSet,int topK)
	{
		std::vector< std::pair<int,double> > resultSet;

		std::set<int>::const_iterator i = docSet.begin();
		while( i != docSet.end() )
		{
			double score = 0;

			for(size_t w=0;w<word_tokens.size();++w){


				int wordCnt = _indexPtr->_docMetaData[*i]._wordCount[word_tokens[w]];
				int maxFreq = _indexPtr->_docMetaData[*i]._maxFreqWordCount;

				double tf = 0.5 + ( ( 0.5 * wordCnt )/ maxFreq );
				double idf =  log( (double)_totalDocs/docSet.size());
				//std::cout<<"doc: "<<*i<<" : "<<word_tokens[w]<<" : "<<_indexPtr->_docMetaData[*i]._wordCount[word_tokens[w]];
				//std::cout<<" max: " <<maxFreq<<" tf: "<<tf<<" total docs: " <<_totalDocs<<" docsize:"<<docSet.size()<<" idf: "<<idf<<std::endl;

				score += tf * idf;
			}

			resultSet.push_back( std::make_pair(*i,score));
			++i;
		}

		std::nth_element(resultSet.begin(),resultSet.begin() + topK ,resultSet.end(),ResultSetComparer());
		return resultSet;
	}


	/*
	This api outputs the topK results to the given stream
	*/
	void PrintTitle(std::vector<std::pair<int,double> >& results, int topK)
	{
		if(results.size() == 0){
			std::cout<<"** No matching document found **"<<std::endl;
		}
		for(size_t i=0;i<results.size() && i<topK;++i)
		{
			//std::cout<<_indexPtr->_docMetaData[results[i].first]._title<<" : " <<results[i].second<<endl;
			std::cout<<"["<<i<<"]"<<_indexPtr->_docMetaData[results[i].first]._title<<std::endl;
		}
	}


	/*
	Required for Boost Serialization
	*/
	template<class Archive>
	void serialize(Archive& ar,const unsigned int version)
	{
		ar & *_indexPtr;
		ar & _totalDocs;
	}


};

char* IndexBuilder::delim = "\t";



void PrintUsage()
{
	std::cerr<<"Usage: "<<std::endl;
	std::cerr<<"For buidling index run: CorpusBuilder.exe build <path_to_tsv_file>"<<std::endl;
	std::cerr<<"For querying index run: CorpusBuilder.exe serve <path_to_index_file>"<<std::endl;
}

void StartBuilder(const char* filename)
{
	IndexBuilder builder(filename);
	std::cerr<<"building index ... "<<std::endl;
	builder.BuildIndex();

	boost::filesystem::path p(filename);

	boost::filesystem::path p2 = p.remove_filename();
	p2 = p2 / (boost::filesystem::path("_index.bin"));

	std::ofstream serializedIndex(p2.string(),std::ios::binary);
	serializedIndex.sync_with_stdio(false);
	boost::archive::binary_oarchive oa(serializedIndex,std::ios::binary);
	oa & builder;

	serializedIndex.flush();
	serializedIndex.close();
	std::cerr<<"index build complete, written to file: "<<p2.string()<<std::endl;
}

void StartServer(const char* indexFileName)
{
	std::cerr<<"loading index ..."<<std::endl;

	std::ifstream serializedIndex(indexFileName,std::ios::binary);
	if(!serializedIndex.good()){
		std::cerr<<"Error in loading: "<<indexFileName<<std::endl;
		exit(-1);
	}

	serializedIndex.sync_with_stdio(false);
	boost::archive::binary_iarchive ia(serializedIndex,std::ios::binary);

	IndexBuilder serve("");
	ia & serve;

	std::cerr<<"index loaded ..."<<std::endl<<std::endl;

	std::string line;
	while(1){
		std::cerr<<"Enter query: ";
		getline(std::cin,line);

		if(line.size() == 0)
			break;

		std::vector<std::pair<int,double> >& results = serve.ServeIndex(line,3);
		std::cerr<<" ------------------------"<<std::endl;
		serve.PrintTitle(results,3);
		std::cerr<<" ------------------------"<<std::endl;
		std::cerr<<std::endl;
	}
}





int main(int argc,char* argv[])
{

	if( argc != 3){
		PrintUsage();
		return EXIT_FAILURE;
	}


	if(strcmp(argv[1],"build") == 0){
		StartBuilder(argv[2]);
	}else if( strcmp(argv[1],"serve") == 0){
		StartServer(argv[2]);
	}else{
		PrintUsage();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}