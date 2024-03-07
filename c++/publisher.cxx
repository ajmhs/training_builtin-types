/*
* (c) Copyright, Real-Time Innovations, 2020.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

#include <iostream>

#include <dds/pub/ddspub.hpp>
#include <rti/util/util.hpp>      // for sleep()
#include <rti/config/Logger.hpp>  // for logging
// alternatively, to include all the standard APIs:
//  <dds/dds.hpp>
// or to include both the standard APIs and extensions:
//  <rti/rti.hpp>
//
// For more information about the headers and namespaces, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSNamespaceModule.html
// For information on how to use extensions, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSCpp2Conventions.html

#include "application.hpp"  // for command line parsing and ctrl-c
#include <deque>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

using std::cout;
using dds::core::StringTopicType;
using dds::core::BytesTopicType;
using bytedeque_t = std::deque<uint8_t>;
using std::ifstream;
using std::ostringstream;
using std::string;
using stringvector_t = std::vector<string>;

static const char endl = '\n';
static unsigned int samples_written = 0;
static stringvector_t fortunes = {"In the land of the blind, the one eyed man is king",
                                  "Now is the time for all good men to come to the aid of the party" };

void populate_fortunes() {

    string line;
    ostringstream ss;
    ifstream file("fortunes");

    if (file.is_open()) {
        while (std::getline(file, line)) {
            if (line == "%") {   
                fortunes.push_back(ss.str());
                ss.str("");
            }
            else {
                if (!ss.str().empty())
                    ss << '\n';
                ss << line;
            }
        }
        file.close();
    }    
}

void run_publisher_application(unsigned int domain_id, unsigned int sample_count)
{
    populate_fortunes();

    // DDS objects behave like shared pointers or value types
    // (see https://community.rti.com/best-practices/use-modern-c-types-correctly)

    // Start communicating in a domain, usually one participant per application
    dds::domain::DomainParticipant participant(domain_id);

    // Create a topic to handle built-in String types 
    dds::topic::Topic<StringTopicType> string_topic(participant, "StringTopic");

    // Create a topic to handle built-in Octet types
    dds::topic::Topic<BytesTopicType> octet_topic(participant, "BytesTopic");

    // Create a Publisher
    dds::pub::Publisher publisher(participant);

    // Create a DataWriter for the built-in string type with default QoS
    dds::pub::DataWriter<StringTopicType> string_writer(publisher, string_topic);

    // Create a DataWriter for the built-in octet type with default QoS
    dds::pub::DataWriter<BytesTopicType> bytes_writer(publisher, octet_topic);

    // Create some initial binary data
    bytedeque_t my_data;
    for (int i = 0; i < 256; ++i)
        my_data.push_back(static_cast<uint8_t>(i));

    // 
    dds::core::ByteSeq wire_data;
    wire_data.resize(my_data.size());

    // Main loop, write data    
    for (; !application::shutdown_requested && samples_written < sample_count; ++samples_written) {

        // Write a fortune
        string_writer.write(fortunes[std::rand() % fortunes.size()]);
        
        // Write some binary data
        std::copy(my_data.begin(), my_data.end(), wire_data.begin());
        bytes_writer.write(wire_data);
        
        // Shuffle the binary data        
        my_data.push_back(my_data.front());
        my_data.pop_front();
        
        rti::util::sleep(dds::core::Duration(3));
    }
}

int main(int argc, char *argv[])
{
    using namespace application;

    std::srand(std::time(0)); 

    // Parse arguments and handle control-C
    auto arguments = parse_arguments(argc, argv);
    if (arguments.parse_result == ParseReturn::exit) {
        return EXIT_SUCCESS;
    } else if (arguments.parse_result == ParseReturn::failure) {
        return EXIT_FAILURE;
    }
    setup_signal_handlers();

    // Sets Connext verbosity to help debugging
    rti::config::Logger::instance().verbosity(arguments.verbosity);

    try {
        run_publisher_application(arguments.domain_id, arguments.sample_count);
    } catch (const std::exception& ex) {
        // This will catch DDS exceptions
        std::cerr << "Exception in run_publisher_application(): " << ex.what()
        << endl;
        return EXIT_FAILURE;
    }

    // Releases the memory used by the participant factory.  Optional at
    // application exit
    dds::domain::DomainParticipant::finalize_participant_factory();

    return EXIT_SUCCESS;
}
