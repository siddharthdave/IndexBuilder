***** Query Execution Workflow *****
1) The user query is tokenized ( using the same wordbreaker as used while indexing documents)
2) The candidate documents are looked up in the hash table for each word , and 'anded' together to form a candidate set.
3) TF-IDF model is applied to calculate {token} - {docuement} score . 
4) Finally top-k documents are returned.


***** Runtime Complexity *****
The overall runtime complexity is a function of:
1) word breaking the query: O(w) , number of words in the query
2) query word[s] lookup: O(1) , since hash table is used.
3) Ranking: O(d) , where 'd' is the number of candidate documents to be ranked.

The space complexity is a function of O(w) - where 'w' represents the number of unique 'tokens' (normalized words) in the 
corpus.


****** Scalability ******
1) The current solution is only bounded by the amount of RAM on the machine , the CPU consumtion for serving query
is very less.
2) With hybrid approaches ( spilling portions of the index on to SSD etc.) the utilization of the machine can be increased even further.



******* RANKING Limitations ********
1) The current ranking model is based on the tf-idf similarity.Hence only the occurence count is taken into account.
2) Since this is a pure BOW model so relative occurence of the tokens is not taken into account.
3) Other signals like - Title match , Number of matching words in a document , stop word removal etc. are not implemented.


***** FUTURE IMPROVEMENTS ******
Infrastructure:
1) Compressed indexes & ability to perform lookup on compressed data.
2) Using a bitmap for document sets 
3) Hybrid index (using SSD to save RAM - without sacrifising performance).

Ranking:
1) Train a machine learned model that takes features like - TFID score, Title match , Body match , Token occurences etc. as features.
2) Implement basic spell correction
3) Better normalization of text 	

