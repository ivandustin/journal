// Journal.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "journal.h"
#include <iostream>

using namespace std;

void handlepage(journal::page_t);
void print(journal::output_t);
void test1();
void test2();
void test3();
void test4();
void test5();
void test6();
void test7();

int _tmain(int argc, _TCHAR* argv[])
{
	test7();
	return 0;
}

void test1() {
	journal j;
	journal::attr_t attr;
	attr["hostname"] = "mnl079win";
	j.onprint = &print;
	j.onpage = &handlepage;
	j.credit(0, "apple", "ivan", 1, attr);
	j.credit(1, "mango", "louise", 1);
	j.debit(5, "mango", "louise", 1);
	j.debit(86400, "apple", "ivan", 1);
}

void test2() {
	journal j;
	j.onprint = &print;
	j.credit(0, "apple", "ivan", 1);
	j.balance(5, "apple", 5);
	j.debit(1800, "apple", "ivan", 1);
}

void test3() {
	journal j;
	j.onprint = &print;
	j.credit(0, "apple", "ivan", 5);
	j.balance(5, "apple", 4);
	j.credit(1800, "apple", "ivan", 1);
}

void test4() {
	journal j;
	j.onprint = &print;
	j.credit(0, "apple", "ivan", 5);
	j.balance(5, "apple", 4);
	j.debit(10, "apple", "ivan", 3);
	j.credit(1800, "apple", "louise", 1);
	j.debit(1810, "apple", "louise", 1);
}

void test5() {
	journal j;
	j.onprint = &print;
	j.credit(0, "apple", "ivan", 5);
	j.balance(5, "apple", 4);
	j.debit(10, "apple", "ivan", 5);
	j.credit(1800, "apple", "louise", 1);
	j.debit(1810, "apple", "louise", 1);
}

void test6() {
	journal j;
	journal::attr_t attr;
	attr["hostname"] = "mnl079win";
	j.onprint = &print;
	j.credit(0, "apple", "ivan", 5, attr);
	j.balance(5, "apple", 4);
	j.debit(10, "apple", "ivan", 5);
	j.credit(1800, "apple", "louise", 1);
	j.debit(1810, "apple", "louise", 1);
	j.save("sample.status");
}

void test7() {
	journal j, j2;
	journal::attr_t attr;
	attr["hostname"] = "mnl079win";
	j.credit(12, "apple", "ivan", 5, attr);
	j.credit(14, "orange", "louise", 1);
	j.debit(3600, "apple", "ivan", 5);
	j.save("sample.status");

	j2.load("sample.status");
	j2.save("sample2.status");
}

void handlepage(journal::page_t page) {
	cout << page.size() << endl;
}

void print(journal::output_t output) {
	cout << output.start << '\t' << output.len << '\t' << output.asset << '\t' << output.user << '\t' << output.value << '\t' << output.attr["hostname"] << endl;
}
