#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

// small program, so this is not a big deal
using namespace std;

// rows matching = g_index[attribute name][attribute value]
static unordered_map<string,unordered_map<string,vector<size_t>>> g_index;

// attribute value = g_rows[row index][attribute name]
static vector<unordered_map<string,string>> g_rows;

// break line into fields
static vector<string> fields(char *line);

int main(int argc, char **argv)
{
        if (argc != 3) {
                fprintf(stderr, "Usage: ./a.out file0 file1\n");
                exit(1);
        }

        auto fp = fopen(argv[1], "r");
        if (fp == NULL) {
                fprintf(stderr, "could not open %s\n", argv[1]);
                exit(1);
        }

        // first line has attribute names
        char buf[BUFSIZ];
        if (fgets(buf, sizeof(buf), fp) == NULL) {
                fprintf(stderr, "%s is empty\n", argv[1]);
                exit(1);
        }
        auto attrnames = fields(buf);

        // read first database into index
        while (fgets(buf, sizeof(buf), fp) != NULL) {
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
        if (ferror(fp)) {
                fprintf(stderr, "could not read %s\n", argv[1]);
                exit(1);
        }
        if (fclose(fp) < 0) {
                fprintf(stderr, "could not close %s\n", argv[1]);
                exit(1);
        }

        fp = fopen(argv[2], "r");
        if (fp == NULL) {
                fprintf(stderr, "could not open %s\n", argv[2]);
                exit(1);
        }

        if (fgets(buf, sizeof(buf), fp) == NULL) {
                fprintf(stderr, "%s is empty\n", argv[2]);
                exit(1);
        }
        auto attrnames2  = fields(buf);

        // find matching attributes
        auto matches = std::vector<size_t>(attrnames.size());
        auto nr_matches = 0;
        size_t i = 0;
        for (; i < attrnames.size(); ++i) {
                for (size_t j = 0; j < attrnames2.size(); ++j) {
                        if (attrnames2[j] == attrnames[i]) {
                                matches[i] = 1;
                                ++nr_matches;
                                break;
                        }
                }
        }
        // if there are any more attributes, then the rest are mismatches
        for (; i < attrnames.size(); ++i)
                matches[i] = 0;
        if (nr_matches == 0) {
                fprintf(stderr, "databases have no matching attributes\n");
                exit(1);
        }

        while (fgets(buf, sizeof(buf), fp) != NULL) {
                auto attrvalues = fields(buf);

                for (size_t attr = 0; attr < attrnames.size(); ++attr) {
                        if (matches[attr] == 0)
                                continue;

                        auto name = attrnames[attr];
                        auto value = attrvalues[attr];
                        auto p = g_index.find(name);
                        if (p->second.size() == 0)
                                continue;

                        auto rows = g_index[name][value];
                        for (size_t r = 0; r < rows.size(); ++r) {
                                for (auto f : g_rows[rows[r]])
                                        printf("%s ", f.second.c_str());
                                for (size_t i = 0; i < attrvalues.size(); ++i)
                                        printf("%s ", attrvalues[i].c_str());
                                printf("\n");
                        }
                }
        }
        if (ferror(fp)) {
                fprintf(stderr, "could not read %s\n", argv[2]);
                exit(1);
        }
        if (fclose(fp) < 0) {
                fprintf(stderr, "could not close %s\n", argv[2]);
                exit(1);
        }
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
