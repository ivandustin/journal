#include "journal.h"
#include <string.h>

#define JOURNAL_MUST(n) if (!(n)) return false
#define JOURNAL_MUST_SCAN(f, s) { if (!tryscan(f, s, sizeof(s) - 1)) return false; }
#define JOURNAL_SCAN(f, s) { if (!tryscan(f, s, sizeof(s) - 1)) return true; }
#define JOURNAL_STRING_FORMAT "%[^" JOURNAL_DELIM JOURNAL_NEWLINE "]"
#define JOURNAL_TIME_FORMAT "%lld"
#define JOURNAL_INT_FORMAT "%d"

namespace journalhelpers {

	bool tryscan(FILE *file, char *str, int len) {
		int from, to;
		bool ret;

		from = ftell(file);
		fscanf(file, str);
		to = ftell(file);
		ret = to >= from + len;
		if (!ret)
			fseek(file, from, SEEK_SET);
		return ret;
	}

	bool readattrs(FILE *file, journal::attr_t &attr) {
		char key[JOURNAL_VALUE_MAX_LEN+1];
		char value[JOURNAL_VALUE_MAX_LEN+1];

		JOURNAL_SCAN(file, "attr" JOURNAL_DELIM);
		JOURNAL_MUST(fscanf(file, JOURNAL_STRING_FORMAT JOURNAL_DELIM, key) == 1);
		JOURNAL_MUST(fscanf(file, JOURNAL_STRING_FORMAT JOURNAL_NEWLINE, value) == 1);
		attr[key] = value;
		return readattrs(file, attr);
	}

	bool readtablevalues(FILE *file, string &asset, string &user, time_t &time, int &value, journal::attr_t &attr) {
		char str[JOURNAL_VALUE_MAX_LEN+1];
		
		JOURNAL_MUST(fscanf(file, JOURNAL_STRING_FORMAT JOURNAL_DELIM, str) == 1);
		asset = str;
		JOURNAL_MUST(fscanf(file, JOURNAL_STRING_FORMAT JOURNAL_DELIM, str) == 1);
		user = str;
		JOURNAL_MUST(fscanf(file, JOURNAL_TIME_FORMAT JOURNAL_DELIM, &time) == 1);
		JOURNAL_MUST(fscanf(file, JOURNAL_INT_FORMAT JOURNAL_NEWLINE, &value) == 1);
		JOURNAL_MUST(readattrs(file, attr));
		return true;
	}
	
	bool readtables(FILE *file, journal::table_t &table) {
		string asset, user;
		time_t time;
		int value;
		journal::attr_t attr;

		JOURNAL_SCAN(file, "table" JOURNAL_DELIM);
		JOURNAL_MUST(readtablevalues(file, asset, user, time, value, attr));
		table[asset][user].time = time;
		table[asset][user].value = value;
		table[asset][user].attr = attr;
		return readtables(file, table);
	}

	bool readpagevalues(FILE *file, time_t &start, time_t &len, string &asset, string &user, int &value, journal::attr_t &attr) {
		char str[JOURNAL_VALUE_MAX_LEN+1];
		
		JOURNAL_MUST(fscanf(file, JOURNAL_TIME_FORMAT JOURNAL_DELIM, &start) == 1);
		JOURNAL_MUST(fscanf(file, JOURNAL_TIME_FORMAT JOURNAL_DELIM, &len) == 1);
		JOURNAL_MUST(fscanf(file, JOURNAL_STRING_FORMAT JOURNAL_DELIM, str) == 1);
		asset = str;
		JOURNAL_MUST(fscanf(file, JOURNAL_STRING_FORMAT JOURNAL_DELIM, str) == 1);
		user = str;
		JOURNAL_MUST(fscanf(file, JOURNAL_INT_FORMAT JOURNAL_NEWLINE, &value) == 1);
		JOURNAL_MUST(readattrs(file, attr));
		return true;
	}
	
	bool readpages(FILE *file, journal::page_t &page) {
		journal::output_t output;

		JOURNAL_SCAN(file, "page" JOURNAL_DELIM);
		JOURNAL_MUST(readpagevalues(file, output.start, output.len, output.asset, output.user, output.value, output.attr));
		page.push_back(output);
		return readpages(file, page);
	}

	void writeattr(ofstream &file, journal::attr_t &attr) {
		for (journal::attr_t::iterator it = attr.begin(); it != attr.end(); it++)
			file << "attr" << JOURNAL_DELIM << it->first << JOURNAL_DELIM << it->second << JOURNAL_NEWLINE;
	}

	void writepage(ofstream &file, journal::page_t &page) {
		for (journal::page_t::iterator p = page.begin(); p != page.end(); p++) {
			file << "page" << JOURNAL_DELIM << p->start << JOURNAL_DELIM << p->len << JOURNAL_DELIM 
				<< p->asset << JOURNAL_DELIM << p->user << JOURNAL_DELIM << p->value << JOURNAL_NEWLINE;
			writeattr(file, p->attr);
		}
	}

	void writetable(ofstream &file, journal::table_t &table) {
		for (journal::table_asset_t::iterator asset = table.begin(); asset != table.end(); asset++) {
			for (journal::table_user_t::iterator user = asset->second.begin(); user != asset->second.end(); user++) {
				journal::table_entry_t entry = user->second;
				file << "table" << JOURNAL_DELIM << asset->first << JOURNAL_DELIM << user->first << JOURNAL_DELIM 
					<< entry.time << JOURNAL_DELIM << entry.value << JOURNAL_NEWLINE;
				writeattr(file, user->second.attr);
			}
		}
	}

};

using namespace journalhelpers;

journal::journal() {
	time_slice = 1800;
	page_slice = 86400;
	time_index = -1;
	t		   = -1;
	onprint	   = NULL;
	onpage	   = NULL;
}

void journal::credit(time_t time, string asset, string user, int value) {
	updatetime(time);
	if (exists(asset, user))
		flushentry(time, asset, user);
	table[asset][user].time = time;
	table[asset][user].value += value;
}

void journal::credit(time_t time, string asset, string user, int value, attr_t attr) {
	credit(time, asset, user, value);
	table[asset][user].attr.insert(attr.begin(), attr.end());
}

void journal::credit(string asset, string user, int value) {
	credit(t, asset, user, value);
}

void journal::credit(string asset, string user, int value, attr_t attr) {
	credit(t, asset, user, value, attr);
}

void journal::debit(time_t time, string asset, string user, int value) {
	updatetime(time);
	if (!exists(asset, user))
		return;
	if (sufficient(asset, user, value)) {
		flushentry(time, asset, user);
		table[asset][user].value -= value;
	} else {
		if (sufficient(asset, JOURNAL_LOST, value))
			debit(time, asset, JOURNAL_LOST, value);
		else
			debit(time, asset, JOURNAL_LOST, table[asset][JOURNAL_LOST].value);
	}
}

void journal::debit(string asset, string user, int value) {
	debit(t, asset, user, value);
}

void journal::balance(time_t time, string asset, int expected) {
	int actual, diff;
	
	updatetime(time);
	actual = count(asset);
	if (actual == expected)
		return;
	if (expected > actual) {
		diff = expected - actual;
		table[asset][JOURNAL_LOST].value = diff;
	} else if (actual > expected) {
		for (table_user_t::iterator it = table[asset].begin(); it != table[asset].end(); it++)
			debit(time, asset, it->first, it->second.value);
		table[asset][JOURNAL_LOST].value = expected;
	}
	table[asset][JOURNAL_LOST].time = time;
}

void journal::balance(string asset, int expected) {
	balance(t, asset, expected);
}

bool journal::sufficient(string asset, string user, int value) {
	return table[asset][user].value >= value;
}

int journal::count(string asset) {
	int sum = 0;
	for (table_user_t::iterator it = table[asset].begin(); it != table[asset].end(); it++)
		sum += it->second.value;
	return sum;
}

void journal::flushentry(time_t time, string asset, string user) {
	struct table_entry *entry;
	struct output output;
	time_t start;
	time_t len;
	int value;

	entry	= &table[asset][user];
	start	= entry->time;
	len		= time - start;
	value	= entry->value;

	output.start	= start;
	output.len		= len;
	output.asset	= asset;
	output.user		= user;
	output.value	= value;
	output.attr		= entry->attr;
	print(output);

	entry->time = time;
}

bool journal::exists(string asset, string user) {
	return table.find(asset) != table.end() && table[asset].find(user) != table[asset].end();
}

time_t journal::timeindex(time_t time) {
	return time - (time % time_slice);
}

time_t journal::pageindex(time_t time) {
	return time - (time % page_slice);
}

void journal::flushall(time_t time) {
	time_t from, to;
	
	from = time_index + time_slice;
	to = timeindex(time);
	for (time_t i = from; i <= to; i += time_slice)
		for (table_asset_t::iterator asset = table.begin(); asset != table.end(); asset++)
			for (table_user_t::iterator user = table[asset->first].begin(); user != table[asset->first].end(); user++)
				flushentry(i, asset->first, user->first);
}

void journal::updatetime(time_t time) {
	time_t new_time_index;

	t = time;
	if (time_index == -1)
		time_index = timeindex(time);
	new_time_index = timeindex(time);
	if (new_time_index != time_index)
		flushall(time);
	time_index = new_time_index;
}

void journal::print(output_t output) {
	struct output last;
	
	if (page.size() > 0) {
		last = page.back();
		if (pageindex(last.start) != pageindex(output.start)) {
			if (onpage != NULL) {
				onpage(page);
				page.clear();
			}
		}
	}

	if (output.len == 0)
		return;
	if (output.value == 0 && output.len != time_slice)
		return;

	page.push_back(output);

	if (onprint != NULL)
		onprint(output);

#ifdef JOURNAL_STDOUT
	cout << output.start << '\t' << output.len << '\t' << output.asset << '\t' << output.user << '\t' << output.value << endl;
#endif
}

bool journal::save(string path) {
	ofstream file;

	file.open(path.c_str(), ios::out);
	JOURNAL_MUST(file.is_open());
	file << "journal state table" << JOURNAL_NEWLINE;
	file << "time saved" << JOURNAL_DELIM << time(NULL) << JOURNAL_NEWLINE;
	file << "time slice" << JOURNAL_DELIM << time_slice << JOURNAL_NEWLINE;
	file << "page slice" << JOURNAL_DELIM << page_slice << JOURNAL_NEWLINE;
	file << "time index" << JOURNAL_DELIM << time_index << JOURNAL_NEWLINE;
	file << "time" << JOURNAL_DELIM << t << JOURNAL_NEWLINE;
	writeattr(file, attr);
	writetable(file, table);
	writepage(file, page);
	file.close();
	JOURNAL_MUST(file);
	return true;
}

bool journal::load(string path) {
	FILE		*file;
	time_t		tsaved		= -1;
	int			tslice		= 0;
	int			pslice		= 0;
	time_t		tindex		= -1;
	time_t		tt			= -1;
	table_t		tb;
	page_t		p;
	attr_t		at;

	file = fopen(path.c_str(), "r");
	JOURNAL_MUST(ferror(file) == 0);
	JOURNAL_MUST_SCAN(file, "journal state table" JOURNAL_NEWLINE);
	JOURNAL_MUST(fscanf(file, "time saved" JOURNAL_DELIM JOURNAL_TIME_FORMAT JOURNAL_NEWLINE, &tsaved) == 1);
	JOURNAL_MUST(fscanf(file, "time slice" JOURNAL_DELIM JOURNAL_INT_FORMAT JOURNAL_NEWLINE, &tslice) == 1);
	JOURNAL_MUST(fscanf(file, "page slice" JOURNAL_DELIM JOURNAL_INT_FORMAT JOURNAL_NEWLINE, &pslice) == 1);
	JOURNAL_MUST(fscanf(file, "time index" JOURNAL_DELIM JOURNAL_TIME_FORMAT JOURNAL_NEWLINE, &tindex) == 1);
	JOURNAL_MUST(fscanf(file, "time" JOURNAL_DELIM JOURNAL_TIME_FORMAT JOURNAL_NEWLINE, &tt) == 1);
	JOURNAL_MUST(readattrs(file, at));
	JOURNAL_MUST(readtables(file, tb));
	JOURNAL_MUST(readpages(file, p));
	time_slice = tslice;
	page_slice = pslice;
	time_index = tindex;
	t		   = tt;
	table	   = tb;
	page	   = p;
	attr	   = at;
	fclose(file);
	JOURNAL_MUST(ferror(file) == 0);
	return true;
}

