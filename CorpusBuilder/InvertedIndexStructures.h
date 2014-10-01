#ifndef INVERTED_INDEX_STRUCTURES_H
#define INVERTED_INDEX_STRUCTURES_H

/*
This defines the underlying data structures for holdin up the index for very fast lookups.
space is traded off for speed.
*/

#include <map>
#include <set>
#include <string>
#include "boost\unordered_map.hpp"

class IndexBuilder;


//gets stored per token in the corpus
struct IndexEntry {
	std::set<int> _docSet;

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar & _docSet;
	}
};

//gets stored per document
struct DocEntry{
	int _docId;
	int _docLength;
	boost::unordered_map<std::string,int> _wordCount;
	boost::unordered_map<std::string, std::pair<int,int> > _wordOffsets; //curretly kept as empty - can be used for caption generation
	int _maxFreqWordCount;
	std::string _title;
	std::string _body; // currently kept as empty - can be used for caption generation
	DocEntry(int docId,char* title,char* body):_docId(docId),_docLength(0),_maxFreqWordCount(0),_title(title),_body(""){}
	DocEntry(){}

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version)
	{
		ar & _docId;
		ar & _docLength;
		ar & _wordCount;
		ar & _wordOffsets;
		ar & _maxFreqWordCount;
		ar & _title;
		ar & _body;
	}

};

/*
the main data strcuture for holding things in-memory
consists of:
1) {token} - [document set that has the given token]
2) {docId} - [tokens in the given document]
*/
class InvertedIndex{
	friend class IndexBuilder;
private:

	boost::unordered_map<std::string, IndexEntry> _wordIndex;
	boost::unordered_map<int, DocEntry> _docMetaData;


public:
	template<class Archive>
	void serialize(Archive& ar,const unsigned int version)
	{
		ar & _wordIndex;
		ar & _docMetaData;
	}

};

#endif