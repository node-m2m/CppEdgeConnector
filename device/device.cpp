/*
 * File:   device.ccp
 * Author: Ed Alegrid
 *
 * A simple C++ TCP edge connector using epoll with non-blocking read timeout.
 * Use any Linux C++20 compliant compiler or IDE to compile the application.
 *
 */

#include <memory>
#include <iostream>
#include <nlohmann/json.hpp>
#include "lib/server.h"

using namespace std;
using json = nlohmann::json;

int main()
{
    auto getRandomData = [](auto j)
    {
        int rn = rand() % 100 + 10;
        string rd = to_string(rn);
        j["value"] = rd;
        return j.dump(); 
    };

    cout << "\n*** C++ Tcp Edge Connector Server ***\n" << endl;

    auto s = make_shared<Tcp::Server>(5300);

    cout << "Server listening on: " << s->ip << ":" << s->port << endl;

    for (;;)
    {
        try{
            // wait for a client connection
            s->wait();
          
            // read rcvd data from a client
            auto data = s->read();

            // parse rcvd json string data
            try{
                auto j = json::parse(data);

                if(j["topic"] == "random-data"){
                    auto rd = std::async(getRandomData, j);
                    rd.wait();
                    auto r = rd.get();
                    s->write(r);
                    cout << "json string result: " << r << '\n';  
                }
                else{
                    cout << "invalid topic:" << s->write("invalid topic") << endl;
                }
                s->end();
            }
            catch (json::parse_error& ex)
            {
                // rcvd data is not a json string 
                cerr << "json parse error at byte: " << ex.byte << endl;
                cout << "rcvd an invalid json data: " << endl;
                s->write("invalid json data"); 
                s->end();
            }
        }
        catch (SocketError& e)
        {
            cerr << "error: " << e.what() << endl;
            exit(1);
        }
    }
  
    return 0;
}
