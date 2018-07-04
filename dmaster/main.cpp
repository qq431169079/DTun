#include "Server.h"
#include "Logger.h"
#include "DTun/SignalHandler.h"
#include "DTun/SignalBlocker.h"
#include <boost/make_shared.hpp>
#include <log4cplus/configurator.h>
#include <iostream>

using namespace DMaster;

boost::shared_ptr<Server> server;

static void signalHandler(int sig)
{
    LOG4CPLUS_INFO(logger(), "Signal " << sig << " received");
    boost::shared_ptr<Server> tmp = server;
    if (tmp) {
        tmp->stop();
        server.reset();
    }
}

int main(int argc, char* argv[])
{
    log4cplus::helpers::Properties props;

    props.setProperty("log4cplus.rootLogger", "TRACE, console");
    props.setProperty("log4cplus.appender.console", "log4cplus::ConsoleAppender");
    props.setProperty("log4cplus.appender.console.layout", "log4cplus::PatternLayout");
    props.setProperty("log4cplus.appender.console.layout.ConversionPattern", "%-5p %c - %m%n");

    log4cplus::PropertyConfigurator propConf(props);
    propConf.configure();

    DTun::SignalBlocker signalBlocker;

    DTun::SignalHandler sigHandler(&signalHandler);

    UDT::startup();

    boost::shared_ptr<Server> server_tmp = boost::make_shared<Server>(3030);

    if (!server_tmp->start()) {
        UDT::cleanup();
        return 1;
    }

    server = server_tmp;

    server_tmp->run();

    server.reset();
    server_tmp.reset();

    UDT::cleanup();

    LOG4CPLUS_INFO(logger(), "Done!");

    return 0;
}
