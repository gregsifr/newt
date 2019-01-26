#include "SimpleAwkParser.h"

// Simple test of SimpleAwkParser
int main(int argc, char** argv) {
    SimpleAwkParser p;
    p.setFS("[ ,\t]+");
    if (!p.openF("test.txt"))
        printf("could not open\n");
    while (p.getLine()) {
        printf("%d %d\n", p.NR(), p.NF());
        for (int i=1; i <= p.NF(); i++)
            printf("\t[%s]\n", p.getField(i).c_str());
    }
    
    p.closeF();
}
