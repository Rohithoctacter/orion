#include <iostream>
#include <map>
using namespace std;

map<string, string> returnTypes;

void process_function(string name) {
    cout << "Processing function: " << name << endl;
    returnTypes[name] = "list";
    cout << "Recorded return type for " << name << ": " << returnTypes[name] << endl;
}

void use_function(string name) {
    cout << "Using function: " << name << endl;
    if (returnTypes.find(name) != returnTypes.end()) {
        cout << "Found return type: " << returnTypes[name] << endl;
    } else {
        cout << "No return type found!" << endl;
    }
}

int main() {
    process_function("make_list");
    use_function("make_list");
    return 0;
}
