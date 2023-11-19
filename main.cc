#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <err.h>
#include <string>
#include <sysexits.h>
#include <unordered_map>
#include <vector>

// my only real reason for using c++ is because of the
// easy access to data structures rather then C where you have
// to implement hashmaps, dynamic arrays, etc yourself

// normally you should NOT do this, but its a small program so it
// is not a big deal
using namespace std;

// these are the two databases that will be used for examples
//
// database0
// ---------
// ID   NAME    JOB
// 0    ethan   programmer
// 1    chris   programmer
// 2    sonny   nuke_maker
// 3    ethan   unemployed
//
// datebase1
// ---------
// ID   PAY
// 0    0.00
// 1    10000000000.00
// 2    1000000000000000000.00
// 3    0.00

// usage
// -----
// firstme = g_index["NAME"]["ethan"][0] see below for discussion on g_index
// myid = g_rows[firstme]["ID"]
//
// implementation
// --------------
// every tuple (row) of the database is a mapping from attribute
// name to attribute value. we represent this as an array of
// hash tables where these tables are indexed on attribute
// name
//
// [
//      {
//              "NAME": "ethan",
//              "ID":   "0"
//      },
//      {
//              "NAME": "chris",
//              "ID":   "1"
//      },
//      {
//              "NAME": "sonny",
//              "ID:    "2"
//      },
//      {
//              "NAME": "ethan",
//              "ID":   "3"
//      }
// ]
static vector<unordered_map<string,string>> g_rows;


// usage
// -----
// lets get the IDs of everyone name "ethan"
//
// rows = g_index["NAME"]["ethan"]
// for row in rows:
//      row = g_rows[row]
//      print row["ID"]
//
// implementation
// --------------
// these are the resulting data structures after we have
// built the index from the first file given
//
// g_index = {
//      "ID": {
//              "0": [ 0 ],
//              "1": [ 1 ],
//              "2": [ 2 ],
//              "3": [ 3 ],
//      },
//      "NAME": {
//              see how a list is made for all rows that
//              have the NAME "ethan"
//              "ethan": [ 0, 3 ],
//              "chris": [ 1 ],
//              "sonny": [ 2 ],
//      }
// }
//
// i keep the rows separate from the index so updates do not
// have to find all copys of a row in g_index. i could use a separate
// mapping from attribute value to rows that have to be updated, but
// this is simpler. for example, lets say i did implement it as that
//
// g_index = {
//      "ID": {
//              "0": [ {"NAME":"ethan","ID":"0"} ],
//              "1": [ {"NAME":"chris","ID":"1"} ],
//              "2": [ {"NAME":"sonny","ID":"2"} ],
//              "3": [ {"NAME":"ethan","ID":"3"} ],
//      },
//      "NAME": {
//              "ethan": [ {"NAME":"ethan","ID":"0"}, {"NAME":"ethan","ID":"3"} ],
//              "chris": [ {"NAME":"chris","ID":"1"} ],
//              "sonny": [ {"NAME":"sonny","ID":"2"} ],
//      }
// }
//
// so what happens if i want to update my "NAME" to "Ethan"? well, i would
// have to go through all copys of my row and update all of them. however,
// keeping them in a separate array makes updates very easy
//
// rowidx = g_index["NAME"][ethan][0]
// g_rows[rowidx]["NAME"] = "Ethan"
static unordered_map<string,unordered_map<string,vector<size_t>>> g_index;

// break line into fields, just what awk does
static vector<string> fields(const string &line);

// versions of stdio functions that print error messages
// and exit() on failures, the path of the file is passed to
// all these functions for better error messages
static FILE *e_fopen(const char *path, const char *mode);
static void e_fclose(const char *path, FILE *fp);

// get first line of database, which contains the attribute names
static vector<string> db_first(const char *path, FILE *fp);

// read a line into a std::string, no limit on line size
static string read_line(const char *path, FILE *fp);

// get index of attribute
static size_t attr_index(const vector<string> &attrs, const string &attr);

int main(int argc, char **argv)
{
        if (argc != 4) {
                fprintf(stderr, "Usage: ./a.out file0 file1 attribute\n");
                exit(1);
        }

        // first database
        auto first = argv[1];
        // second database
        auto second = argv[2];
        // attribute that we are joining on
        auto join_attr = argv[3];

        // we read the first file into the index, but
        // do not create an index for the second file. we
        // iterate through the lines of the second file
        // and join them on the index made by the first file
        auto fp = e_fopen(first, "r");

        // first line has attribute names
        auto attrnames = db_first(first, fp);

        // make sure first file has join attribute
        if (attr_index(attrnames, join_attr) == attrnames.size()) {
                fprintf(stderr, "%s does not have attribute %s\n", first, join_attr);
                exit(1);
        }

        // read first database into index
        string line;
        while ((line = read_line(first, fp)).size() > 0) {
                // parse attribute values
                auto attrvalues = fields(line);

                // allocate a new row
                g_rows.push_back({});
                auto rowidx = g_rows.size() - 1;

                // add our attributes to the index
                for (size_t attr = 0; attr < attrnames.size(); ++attr) {
                        auto name = attrnames[attr];
                        auto value = attrvalues[attr];

                        // add another row with this attribute value
                        g_index[name][value].push_back(rowidx);

                        // set the attribute in the row as well
                        g_rows[rowidx][name] = value;
                }
        }
        e_fclose(first, fp);

        // time to deal with second file
        fp = e_fopen(second, "r");

        // get attributes of second database. TODO: not a good name
        auto attrnames2 = db_first(second, fp);

        // make sure second database has attribute, and get
        // index of field we are joining on
        auto attridx = attr_index(attrnames2, join_attr);
        if (attridx == attrnames2.size()) {
                fprintf(stderr, "%s does not have attribute %s\n", second, join_attr);
                exit(1);
        }

        // we also use this alot so i lifted it from the loop
        auto join_attr_name = attrnames2[attridx];

        // actual hash join
        while ((line = read_line(second, fp)).size() > 0) {
                auto attrvalues = fields(line);

                // see if there are any rows that have this
                // attribute with this value
                auto value = attrvalues[attridx];
                auto p = g_index[join_attr_name].find(value);
                if (p == g_index[join_attr_name].end())
                        continue;

                // iterate through all rows that match
                auto rows = g_index[join_attr][value];
                for (size_t r = 0; r < rows.size(); ++r) {
                        // first files attributes
                        for (auto attr : g_rows[rows[r]])
                                printf("%s ", attr.second.c_str());

                        // second files attributes minus the one we are joining on
                        for (size_t i = 0; i < attrvalues.size(); ++i) {
                                if (i != attridx)
                                        printf("%s ", attrvalues[i].c_str());
                        }
                        printf("\n");
                }
        }
        e_fclose(second, fp);
}

static FILE *e_fopen(const char *path, const char *mode)
{
        auto fp = fopen(path, mode);
        if (fp == NULL)
                err(EX_SOFTWARE, "could not open %s", path);
        return fp;
}

static void
e_fclose(const char *path, FILE *fp)
{
        if (fclose(fp) < 0)
                err(EX_SOFTWARE, "could not close %s", path);
}

static vector<string> db_first(const char *path, FILE *fp)
{
        return fields(read_line(path, fp));
}

static string read_line(const char *path, FILE *fp)
{
        string line = "";
        int c;

        // keep reading characters until we hit
        // end of file or a newline. TODO: this could
        // be inefficient but stdio does its own buffering,
        // so i don't even know if implementing it myself would
        // be faster. id have to do all the things they do anyways
        while ((c = fgetc(fp)) != EOF) {
                line += c;
                if (c == '\n')
                        break;
        }
        if (ferror(fp))
                err(EX_SOFTWARE, "could not read %s", path);

        return line;
}

static vector<string> fields(const string &line)
{
        vector<string> f;
        string cur;
        cur = "";

        for (auto c : line) {
                if (!isspace(c)) {
                        cur += c;
                } else if (cur.size() > 0) {
                        f.push_back(cur);
                        cur = "";
                }
        }

        return f;
}

static size_t attr_index(const vector<string> &attrs, const string &attr)
{
        size_t i;

        // simple linear search over attribute names. TODO switch to
        // a hash set in order to have better big O on attribute name
        // lookups. they only reason i have it as a basic list is
        // because it doesn't seem like any one table is gunna have so
        // many attributes that a linear search of an array is going
        // to be a bottleneck. having it as an array also makes iteration
        // cheaper, because iterating through a hash map is gunna take
        // longer than through a simple array
        for (i = 0; i < attrs.size(); ++i) {
                if (attrs[i] == attr)
                        break;
        }

        return i;
}
