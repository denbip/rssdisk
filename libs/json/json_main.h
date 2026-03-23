#ifndef JSON_MAIN_H
#define JSON_MAIN_H

#include "json/json.h"
#include <string>
#include <iostream>

class json_main
{
public:
    json_main()
    {

    }

    void test()
    {
        std::string config_doc =
        "{"
            "\"encoding\" : \"UTF-8\","

            // Plug-ins loaded at start-up
            "\"plug-ins\" : ["
                "\"python\","
                "\"c++\","
                "\"ruby\""
                "],"

            // Tab indent size
            "\"indent4\" : { \"length\" : 3, \"use_space\": true }"
        "}";

        Json::Value root;   // will contains the root value after parsing.
        Json::Reader reader;
        /*bool parsingSuccessful = reader.parse( config_doc, root );
        if ( !parsingSuccessful )
        {
            // report to the user the failure and their locations in the document.
            std::cout  << "Failed to parse configuration\n"
                       << reader.getFormattedErrorMessages();
            return;
        }*/

        // Get the value of the member of root named 'encoding', return 'UTF-8' if there is no
        // such member.
        std::string encoding = root.get("encoding", "UTF-8" ).asString();
        // Get the value of the member of root named 'encoding', return a 'null' value if
        // there is no such member.
        const Json::Value plugins = root["plug-ins"];
        for ( int index = 0; index < plugins.size(); ++index )  // Iterates over the sequence elements.
           std::cout << plugins[index].asString() << std::endl;

        std::cout << root["indent"].get("length", 4).asInt() << std::endl;
        std::cout << root["indent"].get("use_space", true).asBool() << std::endl;

        // ...
        // At application shutdown to make the new configuration document:
        // Since Json::Value has implicit constructor for all value types, it is not
        // necessary to explicitly construct the Json::Value object:
        root["encoding"] = "poe\"be\n";
        root["indent"]["length"] = 5;
        root["indent2"]["use_space"] = 23;
        root["fjhrtj"]["use_space"]["ert"] = true;

        Json::StyledWriter writer;
        // Make a new JSON document for the configuration. Preserve original comments.
        std::string outputConfig = writer.write( root );

        // And you can write to a stream, using the StyledWriter automatically.
        std::cout << root << std::endl;

        std::cout << outputConfig << std::endl;
    }
};

#endif // JSON_MAIN_H
