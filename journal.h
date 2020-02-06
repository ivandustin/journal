#include <iostream>
#include <ctime>
#include <string>
#include <map>
#include <vector>
#include <fstream>

using namespace std;

#define JOURNAL_LOST "LOST"
#define JOURNAL_DELIM "\t"
#define JOURNAL_NEWLINE "\n"
#define JOURNAL_VALUE_MAX_LEN 512

class journal {

public:
	typedef map<string, string> attr_t;
	struct table_entry {
		time_t	time;
		int		value;
		attr_t	attr;
		table_entry() : time(-1), value(0) {}
	};
	struct output {
		time_t	start;
		time_t	len;
		string	asset;
		string	user;
		int		value;
		attr_t	attr;
	};
	typedef struct table_entry table_entry_t;
	typedef map<string, table_entry_t> table_user_t;
	typedef map<string, table_user_t> table_t;
	typedef table_t table_asset_t;
	typedef struct output output_t;
	typedef vector<output_t> page_t;

public:
	int time_slice;
	page_t page;
	void (*onprint)(output_t);
	void (*onpage)(page_t);

private:
	time_t time_index;
	table_t table;
	string state_path;

	void flushentry(time_t time, string asset, string user);
	void print(struct output);
	bool exists(string asset, string user);
	time_t timeindex(time_t time);
	time_t dayindex(time_t time);
	void flushall(time_t time);
	void updatetime(time_t time);
	int count(string asset);
	bool sufficient(string asset, string user, int value);
	

public:
	journal();
	void credit(time_t time, string asset, string user, int value);
	void credit(time_t time, string asset, string user, int value, attr_t attr);
	void debit(time_t time, string asset, string user, int value);
	void balance(time_t time, string asset, int expected_value);
	void save(string path);
	bool load(string path);

};
