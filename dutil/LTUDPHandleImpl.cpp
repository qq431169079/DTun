#include "DTun/LTUDPHandleImpl.h"
#include "DTun/LTUDPManager.h"
#include "DTun/Utils.h"
#include "Logger.h"
#include <boost/make_shared.hpp>

namespace DTun
{
    LTUDPHandleImpl::LTUDPHandleImpl(LTUDPManager& mgr)
    : mgr_(mgr)
    , pcb_(NULL)
    , pcbLocalPort_(0)
    , pcbRemoteIp_(0)
    , pcbRemotePort_(0)
    , eof_(false)
    , rcvBuff_(TCP_WND)
    {
    }

    LTUDPHandleImpl::LTUDPHandleImpl(LTUDPManager& mgr,
        const boost::shared_ptr<SConnection>& conn, struct tcp_pcb* pcb)
    : mgr_(mgr)
    , pcb_(pcb)
    , pcbLocalPort_(0)
    , pcbRemoteIp_(0)
    , pcbRemotePort_(0)
    , eof_(false)
    , rcvBuff_(TCP_WND)
    , conn_(conn)
    {
        ip_addr_t tcpAddr;
        uint16_t tcpPort;
        err_t err = tcp_tcp_get_tcp_addrinfo(pcb, 1, &tcpAddr, &tcpPort);
        assert(err == ERR_OK);
        pcbLocalPort_ = lwip_htons(tcpPort);
        assert(pcbLocalPort_ != 0);

        tcp_arg(pcb_, this);
        tcp_recv(pcb_, &LTUDPHandleImpl::recvFunc);
        tcp_sent(pcb_, &LTUDPHandleImpl::sentFunc);
        tcp_err(pcb_, &LTUDPHandleImpl::errorFunc);
    }

    LTUDPHandleImpl::~LTUDPHandleImpl()
    {
        assert(!pcb_);
        assert(pcbLocalPort_ == 0);
        assert(pcbRemoteIp_ == 0);
        assert(pcbRemotePort_ == 0);
        assert(!conn_);
    }

    boost::shared_ptr<SConnection> LTUDPHandleImpl::kill(bool sameThreadOnly, bool abort)
    {
        assert(!sameThreadOnly || mgr_.reactor().isSameThread());
        boost::shared_ptr<SConnection> res = conn_;
        conn_.reset();
        if (pcb_) {
            if (pcbRemoteIp_)
                mgr_.pcbDisconnect(pcbLocalPort_, pcbRemoteIp_, pcbRemotePort_);
            mgr_.pcbUnbind(pcbLocalPort_);
            tcp_arg(pcb_, NULL);
            if (!listenCallback_) {
                tcp_recv(pcb_, NULL);
                tcp_sent(pcb_, NULL);
                tcp_err(pcb_, NULL);
            }
            if (abort) {
                tcp_abort(pcb_);
            } else {
                if (tcp_close(pcb_) != ERR_OK) {
                    LOG4CPLUS_FATAL(logger(), "tcp_close failed");
                    tcp_abort(pcb_);
                }
            }
            pcb_ = NULL;
            pcbLocalPort_ = 0;
            pcbRemoteIp_ = 0;
            pcbRemotePort_ = 0;
        }
        return res;
    }

    void LTUDPHandleImpl::ping(UInt32 ip, UInt16 port)
    {
        boost::shared_ptr<SConnection> conn = conn_;
        if (!conn) {
            return;
        }

        boost::shared_ptr<std::vector<char> > sndBuff =
            boost::make_shared<std::vector<char> >(4);

        (*sndBuff)[0] = 0xAA;
        (*sndBuff)[1] = 0xBB;
        (*sndBuff)[2] = 0xCC;
        (*sndBuff)[3] = 0xDD;

        conn->writeTo(&(*sndBuff)[0], &(*sndBuff)[0] + sndBuff->size(),
            ip, port,
            boost::bind(&LTUDPHandleImpl::onPingSend, _1, sndBuff, ip, port));
    }

    bool LTUDPHandleImpl::bind(SYSSOCKET s)
    {
        assert(!conn_);
        if (conn_) {
            return false;
        }

        conn_ = mgr_.createTransportConnection(s);
        return !!conn_;
    }

    bool LTUDPHandleImpl::bind(const struct sockaddr* name, int namelen)
    {
        assert(!conn_);
        if (conn_) {
            return false;
        }

        conn_ = mgr_.createTransportConnection(name, namelen);
        return !!conn_;
    }

    bool LTUDPHandleImpl::getSockName(UInt32& ip, UInt16& port) const
    {
        if (!conn_) {
            LOG4CPLUS_ERROR(logger(), "socket is not bound");
            return false;
        }

        return conn_->handle()->getSockName(ip, port);
    }

    bool LTUDPHandleImpl::getPeerName(UInt32& ip, UInt16& port) const
    {
        if (!pcb_ || connectCallback_ || listenCallback_ || !conn_) {
            LOG4CPLUS_ERROR(logger(), "socket is not connected");
            return false;
        }

        UInt32 myIp = 0;
        UInt16 myPort = 0;

        if (!conn_->handle()->getSockName(myIp, myPort)) {
            return false;
        }

        ip_addr_t tcpAddr;

        err_t err = tcp_tcp_get_tcp_addrinfo(pcb_, 0, &tcpAddr, &port);
        if (err != ERR_OK) {
            LOG4CPLUS_ERROR(logger(), "cannot get tcp addrinfo");
            return false;
        }

        ip = ip_addr_get_ip4_u32(&tcpAddr);
        port = lwip_htons(port);

        port = mgr_.getMappedPeerPort(myPort, ip, port);

        return true;
    }

    int LTUDPHandleImpl::getTTL() const
    {
        assert(conn_);
        if (!conn_) {
            return SYS_INVALID_SOCKET;
        }
        return conn_->handle()->getTTL();
    }

    bool LTUDPHandleImpl::setTTL(int ttl)
    {
        assert(conn_);
        if (!conn_) {
            return SYS_INVALID_SOCKET;
        }
        return conn_->handle()->setTTL(ttl);
    }

    SYSSOCKET LTUDPHandleImpl::duplicate()
    {
        assert(conn_);
        if (!conn_) {
            return SYS_INVALID_SOCKET;
        }
        return conn_->handle()->duplicate();
    }

    void LTUDPHandleImpl::listen(int backlog, const SAcceptor::ListenCallback& callback)
    {
        assert(mgr_.reactor().isSameThread());
        assert(conn_);
        assert(!pcb_);

        UInt32 ip = 0;
        UInt16 port = 0;

        if (!conn_->handle()->getSockName(ip, port)) {
            return;
        }

        struct tcp_pcb* l = tcp_new_ip_type(IPADDR_TYPE_V4);
        if (!l) {
            LOG4CPLUS_ERROR(logger(), "tcp_new_ip_type failed");
            return;
        }

        setupPCB(l);

        uint16_t pcbPort = mgr_.pcbBindAcceptor(l, port);
        if (!pcbPort) {
            tcp_close(l);
            return;
        }

        if (!(pcb_ = tcp_listen_with_backlog(l, backlog))) {
            LOG4CPLUS_ERROR(logger(), "tcp_listen failed");
            mgr_.pcbUnbind(pcbPort);
            tcp_close(l);
            return;
        }

        tcp_arg(pcb_, this);
        tcp_accept(pcb_, &LTUDPHandleImpl::listenerAcceptFunc);

        pcbLocalPort_ = pcbPort;
        listenCallback_ = callback;
    }

    void LTUDPHandleImpl::connect(const std::string& address, const std::string& port, const SConnector::ConnectCallback& callback)
    {
        assert(mgr_.reactor().isSameThread());
        assert(!pcb_);

        if (!conn_) {
            struct sockaddr_in addr;

            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            conn_ = mgr_.createTransportConnection((struct sockaddr*)&addr, sizeof(addr));
            if (!conn_) {
                callback(ERR_IF);
                return;
            }
        }

        UInt32 localIp = 0;
        UInt16 localPort = 0;

        if (!conn_->handle()->getSockName(localIp, localPort)) {
            callback(ERR_IF);
            return;
        }

        struct tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
        if (!pcb) {
            LOG4CPLUS_ERROR(logger(), "tcp_new_ip_type failed");
            callback(ERR_MEM);
            return;
        }

        setupPCB(pcb);

        uint16_t pcbPort = mgr_.pcbBindConnector(pcb, localPort);
        if (!pcbPort) {
            tcp_close(pcb);
            callback(ERR_IF);
            return;
        }

        addrinfo hints;

        memset(&hints, 0, sizeof(struct addrinfo));

        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* res;

        if (::getaddrinfo(address.c_str(), port.c_str(), &hints, &res) != 0) {
            LOG4CPLUS_ERROR(logger(), "cannot resolve address/port");
            tcp_close(pcb);
            callback(ERR_IF);
            return;
        }

        uint16_t pcbRemotePort = mgr_.pcbConnect(pcbPort, ((const struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr,
            ((const struct sockaddr_in*)res->ai_addr)->sin_port);
        if (!pcbRemotePort) {
            tcp_close(pcb);
            callback(ERR_IF);
            return;
        }

        ip_addr_t addr;
        ip_addr_set_ip4_u32(&addr, ((const struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr);

        tcp_arg(pcb, this);
        tcp_err(pcb, &LTUDPHandleImpl::errorFunc);
        err_t err = tcp_connect(pcb, &addr, lwip_ntohs(pcbRemotePort), &LTUDPHandleImpl::connectFunc);

        if (err != ERR_OK) {
            LOG4CPLUS_ERROR(logger(), "tcp_connect failed: " << (int)err);
            mgr_.pcbDisconnect(pcbPort, ((const struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr, pcbRemotePort);
            mgr_.pcbUnbind(pcbPort);
            tcp_close(pcb);
            freeaddrinfo(res);
            callback(err);
            return;
        }

        pcb_ = pcb;
        pcbLocalPort_ = pcbPort;
        pcbRemoteIp_ = ((const struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
        pcbRemotePort_ = pcbRemotePort;
        connectCallback_ = callback;

        freeaddrinfo(res);
    }

    int LTUDPHandleImpl::write(const char* first, const char* last, int& numWritten)
    {
        assert(mgr_.reactor().isSameThread());

        numWritten = 0;

        if (!pcb_) {
            return ERR_ABRT;
        }

        numWritten = std::min((int)(last - first), (int)tcp_sndbuf(pcb_));

        if (numWritten == 0) {
            return 0;
        }

        err_t err = tcp_write(pcb_, first, numWritten, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            LOG4CPLUS_ERROR(logger(), "tcp_write error " << (int)err);
            numWritten = 0;
            return err;
        }

        err = tcp_output(pcb_);
        if (err != ERR_OK) {
            LOG4CPLUS_ERROR(logger(), "tcp_output error " << (int)err);
        }

        return 0;
    }

    int LTUDPHandleImpl::read(char* first, char* last, int& numRead)
    {
        assert(mgr_.reactor().isSameThread());

        numRead = 0;

        if (!rcvBuff_.empty()) {
            int left = last - first;

            boost::circular_buffer<char>::const_array_range arr = rcvBuff_.array_one();

            int tmp = std::min(left, (int)arr.second);
            memcpy(first, arr.first, tmp);

            first += tmp;
            left -= tmp;
            numRead += tmp;

            arr = rcvBuff_.array_two();

            tmp = std::min(left, (int)arr.second);
            memcpy(first, arr.first, tmp);

            numRead += tmp;

            rcvBuff_.erase_begin(numRead);

            if (!eof_ && pcb_) {
                tcp_recved(pcb_, numRead);
            }
        }

        int err = 0;

        if (eof_) {
            err = DTUN_ERR_CONN_CLOSED;
        } else if (!pcb_) {
            err = ERR_ABRT;
        }

        return err;
    }

    UInt16 LTUDPHandleImpl::getTransportPort() const
    {
        if (!conn_) {
            return 0;
        }

        UInt32 ip;
        UInt16 port;

        if (!conn_->handle()->getSockName(ip, port)) {
            return 0;
        }

        assert(ip == htonl(INADDR_ANY));

        return port;
    }

    err_t LTUDPHandleImpl::listenerAcceptFunc(void* arg, struct tcp_pcb* newpcb, err_t err)
    {
        LOG4CPLUS_TRACE(logger(), "LTUDP accept(" << (int)err << ")");

        LTUDPHandleImpl* this_ = (LTUDPHandleImpl*)arg;

        assert(err == ERR_OK);

        this_->setupPCB(newpcb);

        this_->listenCallback_(this_->mgr_.createStreamSocket(this_->conn_, newpcb));

        return ERR_OK;
    }

    err_t LTUDPHandleImpl::connectFunc(void* arg, struct tcp_pcb* pcb, err_t err)
    {
        LOG4CPLUS_TRACE(logger(), "LTUDP connect(" << (int)err << ")");

        LTUDPHandleImpl* this_ = (LTUDPHandleImpl*)arg;

        SConnector::ConnectCallback cb = this_->connectCallback_;
        this_->connectCallback_ = SConnector::ConnectCallback();
        if (err == ERR_OK) {
            tcp_recv(this_->pcb_, &LTUDPHandleImpl::recvFunc);
            tcp_sent(this_->pcb_, &LTUDPHandleImpl::sentFunc);
        }
        cb(err);

        return ERR_OK;
    }

    err_t LTUDPHandleImpl::recvFunc(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err)
    {
        LOG4CPLUS_TRACE(logger(), "LTUDP recv(" << (p ? p->tot_len : -1) << ", " << (int)err << ")");

        assert(err == ERR_OK);

        LTUDPHandleImpl* this_ = (LTUDPHandleImpl*)arg;

        if (!p) {
            this_->eof_ = true;
        } else {
            if (p->tot_len > (this_->rcvBuff_.capacity() - this_->rcvBuff_.size())) {
                pbuf_free(p);
                LOG4CPLUS_FATAL(logger(), "too much data");
                return ERR_MEM;
            }

            struct pbuf* tmp = p;
            do {
                this_->rcvBuff_.insert(this_->rcvBuff_.end(),
                    (char*)tmp->payload, (char*)tmp->payload + tmp->len);
            } while ((tmp = tmp->next));
            pbuf_free(p);
        }

        if (this_->readCallback_) {
            this_->readCallback_();
        }

        return ERR_OK;
    }

    err_t LTUDPHandleImpl::sentFunc(void* arg, struct tcp_pcb* pcb, u16_t len)
    {
        LOG4CPLUS_TRACE(logger(), "LTUDP sent(" << len << ")");

        LTUDPHandleImpl* this_ = (LTUDPHandleImpl*)arg;

        if (this_->writeCallback_) {
            this_->writeCallback_(0, len);
        }

        return ERR_OK;
    }

    void LTUDPHandleImpl::errorFunc(void* arg, err_t err)
    {
        LTUDPHandleImpl* this_ = (LTUDPHandleImpl*)arg;

        if (this_->pcbRemoteIp_)
            this_->mgr_.pcbDisconnect(this_->pcbLocalPort_, this_->pcbRemoteIp_, this_->pcbRemotePort_);
        this_->mgr_.pcbUnbind(this_->pcbLocalPort_);
        this_->pcb_ = NULL;
        this_->pcbLocalPort_ = 0;
        this_->pcbRemoteIp_ = 0;
        this_->pcbRemotePort_ = 0;

        if (this_->connectCallback_) {
            LOG4CPLUS_TRACE(logger(), "LTUDP error(" << (int)err << ")");
            SConnector::ConnectCallback cb = this_->connectCallback_;
            this_->connectCallback_ = SConnector::ConnectCallback();
            cb(err);
        } else {
            if (err == ERR_ABRT) {
                LOG4CPLUS_ERROR(logger(), "LTUDP error(" << (int)err << ")");
            } else {
                LOG4CPLUS_TRACE(logger(), "LTUDP error(" << (int)err << ")");
            }

            if (this_->writeCallback_) {
                this_->writeCallback_(err, 0);
            }
            if (this_->readCallback_) {
                this_->readCallback_();
            }
        }
    }

    void LTUDPHandleImpl::onPingSend(int err, const boost::shared_ptr<std::vector<char> >& sndBuff, UInt32 ip, UInt32 port)
    {
        LOG4CPLUS_TRACE(logger(), "LTUDP onPingSend(" << err << ", " << ipPortToString(ip, port) << ")");
    }

    void LTUDPHandleImpl::setupPCB(struct tcp_pcb* pcb)
    {
        ip_set_option(pcb, SOF_REUSEADDR);
        ip_set_option(pcb, SOF_KEEPALIVE);

        pcb->keep_intvl = 5000;
        pcb->keep_cnt = 4;
        pcb->keep_idle = 10000;
    }
}
