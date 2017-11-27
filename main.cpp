#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <thread>

#include "server.h"

using namespace std;
namespace po = boost::program_options;

int main(int argc, const char** argv) {
    po::options_description desc;
    desc.add_options()
            ("help,h", "Help screen")
            ("version,v", "Version of the application")
            ("port,p", po::value<int>(), "The port the server will listen on")
            ("numworker,n", po::value<int>(), "The number of worker threads")
            ("timeout,t", po::value<int>(), "The time (seconds) to wait for response from server")
            ("log,l", po::value<string>(), "The path to logs");


    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    } catch (exception& e) {
        std::cout << "error: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }
    po::notify(vm);

    if (vm.count("help"))
        cout << desc << endl;
    if (vm.count("version"))
        cout << "Version: 0.1" << endl;

    int port = vm.count("port") ? vm["port"].as<int>() : 8080;
    int numworker = vm.count("numworker") ? vm["numworker"].as<int>() : (int)std::thread::hardware_concurrency();
    int timeout = vm.count("timeout") ? vm["timeout"].as<int>() : 10;
    string log = vm.count("log") ? vm["log"].as<string>() : "log";

    cout << "Server Configuration: \n"
         << "port: " << port << endl
         << "numworker: " << numworker << endl
         << "timeout: " << timeout << endl
         << "log: " << log << endl;


    Server s(port, numworker, timeout, log);
    return 0;
}