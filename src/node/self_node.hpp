#pragma once

#ifndef _node_self_node_hpp
#define _node_self_node_hpp

#include "asio_win32_check.hpp"

#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <string>
#include <ctime>

#include "../interp/interpreter.hpp"
#include "connection.hpp"
#include "address_book.hpp"

namespace prologcoin { namespace node {

class self_node_exception : public std::runtime_error {
public:
    self_node_exception(const std::string &msg)
	: std::runtime_error("self_node_exception: " + msg) { }
};

class self_node;

class address_book_wrapper
{
public:
    address_book_wrapper(address_book_wrapper &&other)
      : self_(other.self_),
	book_(other.book_) { }

    address_book_wrapper(self_node &self, address_book &book);
    ~address_book_wrapper();

    inline address_book & operator ()() { return book_; }

private:
    self_node &self_;
    address_book &book_;
};

class self_node {
private:
    using io_service = boost::asio::io_service;
    using utime = prologcoin::common::utime;
    using term = prologcoin::common::term;
    using term_env = prologcoin::common::term_env;

    friend class connection;
    friend class address_book_wrapper;

public:
    static const int VERSION_MAJOR = 0;
    static const int VERSION_MINOR = 10;

    static const unsigned short DEFAULT_PORT = 8783;
    static const size_t MAX_BUFFER_SIZE = 65536;
    static const size_t DEFAULT_NUM_STANDARD_OUT_CONNECTIONS = 8;
    static const size_t DEFAULT_NUM_VERIFIER_CONNECTIONS = 3;
    static const size_t DEFAULT_NUM_DOWNLOAD_ADDRESSES = 100;

    self_node(unsigned short port = DEFAULT_PORT);

    inline term_env & env() { return env_; }

    inline const std::string & id() const { return id_; }

    inline unsigned short port() const { return endpoint_.port(); }

    // Must be a Prolog term
    void set_comment(const std::string &str);
    inline term get_comment() const { return comment_; }

    address_book_wrapper book() {
	return address_book_wrapper(*this, address_book_);
    }

    inline void set_master_hook(const std::function<void (self_node &)> &hook)
    { master_hook_ = hook; }

    void start();
    void stop();
    void join();
    template<uint64_t C> inline bool join( common::utime::dt<C> t ) {
	return join_us(t);
    }

    inline uint64_t get_timer_interval_microseconds() const {
	return timer_interval_microseconds_;
    }
    inline uint64_t get_fast_timer_interval_microseconds() const {
	return fast_timer_interval_microseconds_;
    }

    // Makes it easier to write fast unit tests that quickly propagate
    // addresses.
    inline bool address_downloader_fast_mode() const {
	return address_downloader_fast_mode_;
    }
    inline void set_address_downloader_fast_mode(bool b) {
	address_downloader_fast_mode_ = b;
    }

    template<uint64_t C> inline void set_timer_interval(utime::dt<C> t)
    {
	timer_interval_microseconds_ = t;
	fast_timer_interval_microseconds_ = t / 10;
	timer_.expires_from_now(boost::posix_time::microseconds(
				timer_interval_microseconds_));

    }

    inline size_t get_num_download_addresses() const {
	return num_download_addresses_;
    }

    inline bool is_self(const ip_service &ip) const {
	return self_ips_.find(ip) != self_ips_.end();
    }

    inline void add_self(const ip_service &ip) {
	self_ips_.insert(ip);
    }

    void for_each_in_session( const std::function<void (in_session_state *)> &fn);

    void for_each_standard_out_connection( const std::function<void (out_connection *conn)> &fn);

    in_session_state * new_in_session(in_connection *conn);
    in_session_state * find_in_session(const std::string &id);
    void kill_in_session(in_session_state *sess);
    void in_session_connect(in_session_state *sess, in_connection *conn);

    out_connection * new_standard_out_connection(const ip_service &ip);
    out_connection * new_verifier_connection(const ip_service &ip);

    class locker;
    friend class locker;

    class locker : public boost::noncopyable {
    public:
	inline locker(self_node &node) : lock_(&node.lock_) { lock_->lock(); }
	inline locker(locker &&other) : lock_(std::move(other.lock_)) { }
	inline ~locker() { lock_->unlock(); }

    private:
	boost::recursive_mutex *lock_;
    };

    inline locker locked() {
	return locker(*this);
    }

private:
    bool join_us(uint64_t microsec);

    static const int DEFAULT_TIMER_INTERVAL_SECONDS = 10;

    void disconnect(connection *conn);
    void run();
    void start_accept();
    void start_tick();
    void prune_dead_connections();
    void connect_to(const std::vector<address_entry> &entries);
    void check_out_connections();
    void check_standard_out_connections();
    bool has_standard_out_connection(const ip_service &ip);
    void check_verifier_connections();
    void close(connection *conn);
    void master_hook();

    io_service & get_io_service() { return ioservice_; }

    using endpoint = boost::asio::ip::tcp::endpoint;
    using acceptor = boost::asio::ip::tcp::acceptor;
    using socket = boost::asio::ip::tcp::socket;
    using strand = boost::asio::io_service::strand;
    using socket_base = boost::asio::socket_base;
    using tcp = boost::asio::ip::tcp;
    using deadline_timer = boost::asio::deadline_timer;

    common::term_env env_;

    std::string id_;
    bool stopped_;
    boost::thread thread_;
    io_service ioservice_;
    endpoint endpoint_;
    acceptor acceptor_;
    socket socket_;
    strand strand_;
    deadline_timer timer_;
    common::term comment_;

    std::unordered_set<ip_service> self_ips_;

    in_connection *recent_in_connection_;
    std::unordered_set<connection *> in_connections_;
    std::unordered_set<connection *> out_connections_;
    std::unordered_set<ip_service> out_standard_ips_;

    boost::recursive_mutex lock_;
    std::unordered_map<std::string, in_session_state *> in_states_;
    std::vector<connection *> closed_;

    address_book address_book_;

    std::function<void (self_node &self)> master_hook_;

    size_t preferred_num_standard_out_connections_;
    size_t preferred_num_verifier_connections_;
    size_t num_standard_out_connections_;
    size_t num_verifier_connections_;

    uint64_t timer_interval_microseconds_;
    uint64_t fast_timer_interval_microseconds_;
    size_t num_download_addresses_;

    bool address_downloader_fast_mode_;
};

inline address_book_wrapper::address_book_wrapper(self_node &self, address_book &book) : self_(self), book_(book)
{
    self_.lock_.lock();
}

inline address_book_wrapper::~address_book_wrapper()
{
    self_.lock_.unlock();
}

}}

#endif
