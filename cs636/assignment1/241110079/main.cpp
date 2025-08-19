
#include "commonutil.h"
#include "djit.h"
#include "fasttrack.h"
#include <chrono>
using namespace std;

bool validateArguments(int argc, char* argv[], string& mode, string& file_path) {
    // Check if there are exactly two arguments
    if (argc != 3) {
        cout << "Usage: <program> <mode (djit|fasttrack)> <file_path>" << endl;
        return false;
    }

    mode = argv[1];
    file_path = argv[2];

    // Validate the mode argument
    if (mode != "djit" && mode != "fasttrack") {
        cout << "Invalid mode. Please specify either 'djit' or 'fasttrack'." << endl;
        return false;
    }

    return true;
}

bool checkFileExists(const string& file_path) {
    ifstream file(file_path);
    if (!file) {
        cout << "Error: Unable to open file at " << file_path << endl;
        return false;
    }
    file.close();
    return true;
}

int main(int argc, char* argv[]) {
    string mode;
    // Validate arguments
    if (!validateArguments(argc, argv, mode, file_path)) {
        return 1;
    }

    // Check if file exists
    if (!checkFileExists(file_path)) {
        return 1;
    }

    //Initialize the data structures common to both algorithms (This may take time)
    std::cout << "Intialising the necessary data structures...." <<std::endl;
    initialize();
    std::cout << "Intialisation Complete." << std::endl;

    // Proceed with the appropriate logic for the selected mode
    //  auto start = std::chrono::high_resolution_clock::now();
    if (mode == "djit") {
        // cout << "Running DJIT .... " << endl;
        djit();
    } else if (mode == "fasttrack") {
        // cout << "Running FastTrack .... " << endl;
        fasttrack();
    }
    // auto end = std::chrono::high_resolution_clock::now();
    // std::chrono::duration<double> time_taken = end - start;
    // std::cout << time_taken.count() << " seconds" << std::endl;

    printDataRaces();

    

    return 0;
}
