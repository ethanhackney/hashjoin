#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include <err.h>
#include <sysexits.h>

// small program, so this is not a big deal
using namespace std;

// rows matching = g_index[attribute name][attribute value]
static unordered_map<string,unordered_map<string,vector<size_t>>> g_index;

// attribute value = g_rows[row index][attribute name]
static vector<unordered_map<string,string>> g_rows;

// break line into fields
static vector<string> fields(char *line);

// versions of stdio functions that print error message
// and exit on failures
static FILE *e_fopen(const char *path, const char *mode);
static void e_fclose(FILE *fp);
static char *e_fgets(char *buf, size_t size, FILE *fp);

// get index of attribute
static size_t attr_index(const vector<string> &attrs, const string &attr);

int main(int argc, char **argv)
{
        if (argc != 4) {
                fprintf(stderr, "Usage: ./a.out file0 file1 attribute\n");
                exit(1);
        }

        auto fp = e_fopen(argv[1], "r");

        // first line has attribute names
        char buf[BUFSIZ];
        if (e_fgets(buf, sizeof(buf), fp) == NULL) {
                fprintf(stderr, "%s is empty\n", argv[1]);
                exit(1);
        }
        auto attrnames = fields(buf);

        // make sure first file has join attribute
        if (attr_index(attrnames, argv[3]) == attrnames.size()) {
                fprintf(stderr, "%s does not have attribute %s\n", argv[1], argv[3]);
                exit(1);
        }

        // read first database into index
        while (e_fgets(buf, sizeof(buf), fp) != NULL) {
                auto attrvalues = fields(buf);

                // allocate a new row
                g_rows.push_back({});
                auto rowidx = g_rows.size() - 1;

                // add attributes to index
                for (size_t attr = 0; attr < attrnames.size(); ++attr) {
                        auto name = attrnames[attr];
                        auto value = attrvalues[attr];
                        g_index[name][value].push_back(rowidx);
                        g_rows[rowidx][name] = value;
                }
        }
        e_fclose(fp);

        fp = e_fopen(argv[2], "r");

        // get attributes of next database
        if (e_fgets(buf, sizeof(buf), fp) == NULL) {
                fprintf(stderr, "%s is empty\n", argv[2]);
                exit(1);
        }
        auto attrnames2 = fields(buf);

        // make sure second database has attribute, and get
        // index of field we are joining on
        auto attridx = attr_index(attrnames2, argv[3]);
        if (attridx == attrnames2.size()) {
                fprintf(stderr, "%s does not have attribute %s\n", argv[2], argv[3]);
                exit(1);
        }

        // actual hash join
        auto join_attr = attrnames2[attridx];
        while (e_fgets(buf, sizeof(buf), fp) != NULL) {
                auto attrvalues = fields(buf);

                // see if there are any rows that have this
                // attribute with this value
                auto value = attrvalues[attridx];
                auto p = g_index[join_attr].find(value);
                if (p == g_index[join_attr].end())
                        continue;

                // iterate through all rows that match
                auto rows = g_index[join_attr][value];
                for (size_t r = 0; r < rows.size(); ++r) {
                        for (auto attr : g_rows[rows[r]])
                                printf("%s ", attr.second.c_str());
                        for (size_t i = 0; i < attrvalues.size(); ++i) {
                                // do not print join field twice because
                                // we already printed it when we did
                                // the row
                                if (i != attridx)
                                        printf("%s ", attrvalues[i].c_str());
                        }
                        printf("\n");
                }
        }
        e_fclose(fp);
}

static FILE *
e_fopen(const char *path, const char *mode)
{
        auto fp = fopen(path, mode);

        if (fp == NULL)
                err(EX_SOFTWARE, "fopen(%s, %s)", path, mode);

        return fp;
}

static void
e_fclose(FILE *fp)
{
        if (fclose(fp) < 0)
                err(EX_SOFTWARE, "fclose()");
}

static char *
e_fgets(char *buf, size_t size, FILE *fp)
{
        auto p = fgets(buf, size, fp);

        if (ferror(fp))
                err(EX_SOFTWARE, "fgets()");

        return p;
}

static vector<string> fields(char *line)
{
        vector<string>  fields;
        char            *base = line;
        char            *end = line;

        for (; *end; ++end) {
                if (!isspace(*end))
                        continue;

                if ((end - base) != 0) {
                        *end = 0;
                        fields.push_back({base});
                }

                base = end + 1;
        }
        // if there are leftovers
        if ((end - base) != 0)
                fields.push_back({base});

        return fields;
}

static size_t attr_index(const vector<string> &attrs, const string &attr)
{
        size_t i;

        for (i = 0; i < attrs.size(); ++i) {
                if (attrs[i] == attr)
                        break;
        }

        return i;
}
