#include <cl-util/float_cmp.h>
#include <iostream>

using namespace std;
using namespace clite::util;

int main ( ) {

    typedef clite::util::cmp<1>::hash_map<char>::type map;
    map my_map;

    my_map[1.01] = 'a';
    my_map[0.99] = 'b';
    my_map[1.02] = 'b';
    my_map[1.08] = 'c';
    my_map[1.09] = 'd';
    my_map[1.10] = 'e';
    my_map[1.11] = 'f';
    my_map[1.19] = 'g';

    cout << cmp<1>::eps << endl;
    for (map::iterator i = my_map.begin(); i != my_map.end(); ++i) {
        cout << i->first << ", " << i->second << endl;
    }

    return 0;

}
