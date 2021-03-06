set(SOURCES
    main.cpp
    Logger.cpp
    tun2socks.c
    DNodeTCPClient.c
    DNodeUdpGwClient.c
    DMasterClient.cpp
    DMasterSession.cpp
    DNodeDirectTCPClient.c
    DNodeProxyTCPClient.cpp
    ProxySession.cpp
    RendezvousSession.h
    RendezvousFastSession.cpp
    RendezvousSymmConnSession.cpp
    RendezvousSymmAccSession.cpp
    PortAllocator.cpp
    PortReservation.cpp
    base/DebugObject.c
    base/BLog.c
    base/BPending.c
    base/BLog_syslog.c
    system/BTime.c
    system/BSignal.c
    system/BNetwork.c
    system/BConnection_common.c
    system/BUnixSignal.c
    system/BConnection_unix.c
    system/BDatagram_unix.c
    system/BProcess.c
    system/BInputProcess.c
    system/BThreadSignal.c
    system/BLockReactor.c
    system/BReactor_badvpn.c
    flow/PacketPassFairQueue.c
    flow/PacketPassPriorityQueue.c
    flow/PacketPassConnector.c
    flow/PacketRecvConnector.c
    flow/StreamRecvConnector.c
    flow/PacketRecvBlocker.c
    flow/PacketPassNotifier.c
    flow/PacketBuffer.c
    flow/SinglePacketBuffer.c
    flow/PacketCopier.c
    flow/PacketStreamSender.c
    flow/PacketProtoEncoder.c
    flow/PacketProtoDecoder.c
    flow/PacketProtoFlow.c
    flow/PacketProtoFlowPassthru.c
    flow/SinglePacketSender.c
    flow/BufferWriter.c
    flow/PacketPassInterface.c
    flow/PacketRecvInterface.c
    flow/StreamPassInterface.c
    flow/StreamRecvInterface.c
    flow/RouteBuffer.c
    flow/PacketRouter.c
    flow/LineBuffer.c
    flow/SingleStreamSender.c
    flow/SingleStreamReceiver.c
    flow/StreamPacketSender.c
    flow/StreamPassConnector.c
    flow/PacketPassFifoQueue.c
    udpgw_client/UdpGwClient.c
    udpgw/udpgw.c
    flowextra/PacketPassInactivityMonitor.c
    flowextra/KeepaliveIO.c
    tuntap/BTap.c
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

configure_file(config1.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config1.ini @ONLY)
configure_file(config2.ini ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config2.ini @ONLY)

add_executable(dnode ${SOURCES})

target_link_libraries(dnode dutil lwip ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} rt dl)
