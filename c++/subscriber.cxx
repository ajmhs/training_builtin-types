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

#include <algorithm>
#include <iostream>

#include <dds/sub/ddssub.hpp>
#include <dds/core/ddscore.hpp>
#include <rti/config/Logger.hpp>  // for logging
#include <rti/util/util.hpp>      // for sleep()

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
#include <mutex>
#include <string>

using std::cout;
using std::string;
using dds::core::StringTopicType;
using dds::core::BytesTopicType;

static std::mutex g_mutex;
static const char endl = '\n';

void process_string_data(dds::sub::DataReader<StringTopicType> reader, unsigned int& sample_count) {

    dds::sub::LoanedSamples<StringTopicType> samples = reader.take();

    const std::lock_guard<std::mutex> lock(g_mutex);

    for (auto sample : samples) {
        if (sample.info().valid()) {
            ++sample_count;
            string fortune = sample.data();
            cout << "SR. Sample data: " << fortune << endl;
        }
        else
            cout << "SR. Instance state changed to " << sample.info().state().instance_state() << endl;
    }        
}

void process_byte_data(dds::sub::DataReader<BytesTopicType> reader, unsigned int& sample_count) {

    dds::sub::LoanedSamples<BytesTopicType> samples = reader.take();
    const std::lock_guard<std::mutex> lock(g_mutex);

    for (auto sample : samples) {
        if (sample.info().valid()) {
            ++sample_count;
            
            // The builtin stream writer treats the bytes as signed, so we implement our own version here
            dds::core::ByteSeq wire_data(sample.data());            
            cout << "BR. Sample data: {";
            dds::core::ByteSeq::const_iterator itor = wire_data.begin();
            for (; itor != wire_data.end(); ++itor) {
                if (itor != wire_data.begin())
                    cout << ", ";
                cout << static_cast<unsigned int>(*itor);
            }
            cout << '}' << endl;
        }
        else
            cout << "BR. Instance state changed to " << sample.info().state().instance_state() << endl;
    }    
}

void run_subscriber_application(unsigned int domain_id, unsigned int sample_count)
{
    // DDS objects behave like shared pointers or value types
    // (see https://community.rti.com/best-practices/use-modern-c-types-correctly)

    // Start communicating in a domain, usually one participant per application
    dds::domain::DomainParticipant participant(domain_id);

    // Create a topic to handle built-in String types 
    dds::topic::Topic<StringTopicType> string_topic(participant, "StringTopic");

    // Create a topic to handle built-in Octet types
    dds::topic::Topic<BytesTopicType> octet_topic(participant, "BytesTopic");
    
    // Create a Subscriber with default Qos
    dds::sub::Subscriber subscriber(participant);    
           
    dds::sub::DataReader<StringTopicType> string_reader(subscriber, string_topic);
    dds::sub::DataReader<BytesTopicType> bytes_reader(subscriber, octet_topic);
        
    // Keep track of the number of samples
    unsigned int samples_read = 0;

    // Create a ReadCondition for any data received on the string reader
    dds::sub::cond::ReadCondition string_read_cond(
        string_reader,
        dds::sub::status::DataState::any(),
        [string_reader, &samples_read]() { process_string_data(string_reader, samples_read); });

    // Create a ReadCondition for any data received on the bytes reader
    dds::sub::cond::ReadCondition bytes_read_cond(
        bytes_reader,
        dds::sub::status::DataState::any(),
        [bytes_reader, &samples_read]() { process_byte_data(bytes_reader, samples_read); });

    // WaitSet will be woken when the attached condition is triggered
    dds::core::cond::WaitSet waitset;
    waitset += string_read_cond;
    waitset += bytes_read_cond;

    cout << "Subscriber ready..." << endl;    
    while (!application::shutdown_requested && samples_read < sample_count) {
        
        // Run the handlers of the active conditions. Wait for up to 1 second.
        waitset.dispatch(dds::core::Duration(1));
    }      
}

int main(int argc, char *argv[])
{
    using namespace application;

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
        run_subscriber_application(arguments.domain_id, arguments.sample_count);
    } catch (const std::exception& ex) {
        // This will catch DDS exceptions
        std::cerr << "Exception in run_subscriber_application(): " << ex.what()
        << endl;
        return EXIT_FAILURE;
    }

    // Releases the memory used by the participant factory.  Optional at
    // application exit
    dds::domain::DomainParticipant::finalize_participant_factory();

    return EXIT_SUCCESS;
}
