/**
 * Heavily redesigned version of the ZeroMQ C++ bindings from
 * https://github.com/zeromq/cppzmq
 */

#pragma once

#include <zmq.h>
#include <cassert>
#include <string>
#include <exception>
#include <vector>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <chrono>

#if defined(__linux__) || defined(__APPLE__)
#  include <signal.h>
#endif

namespace zmq {
    typedef zmq_free_fn free_fn;

    class envelope;

    class exception : public std::exception {
    public:
        exception() : value_(zmq_errno()), what_(zmq_strerror(value_)) { }
        exception(const std::string &what) : value_(0), what_(what) { }

        virtual const char *what() const noexcept override {
            return what_.c_str();
        }

        int value() const noexcept { return value_; }

    protected:
        int value_;
        std::string what_;
    };

    inline void zmq_check(int retval) {
        if (retval < 0)
            throw exception();
    }

    class message {
        friend class socket;

    public:
        message() {
            zmq_check(zmq_msg_init(&msg));
        }

        message(message &&rhs) : msg(rhs.msg) {
            zmq_check(zmq_msg_init(&rhs.msg));
        }

        explicit message(size_t size) {
            zmq_check(zmq_msg_init_size(&msg, size));
        }

        message(const void *src, size_t size) {
            zmq_check(zmq_msg_init_size(&msg, size));
            memcpy(data(), src, size);
        }

        message(void *src, size_t size, free_fn *ffn, void *hint = nullptr) {
            zmq_check(zmq_msg_init_data(&msg, src, size, ffn, hint));
        }

        message &operator=(message &&rhs) noexcept {
            std::swap(msg, rhs.msg);
            return *this;
        }

        ~message() noexcept {
            int rc = zmq_msg_close(&msg);
            assert(rc == 0);
            (void) rc;
        }

        void rebuild() {
            zmq_check(zmq_msg_close(&msg));
            zmq_check(zmq_msg_init(&msg));
        }

        void rebuild(size_t size) {
            zmq_check(zmq_msg_close(&msg));
            zmq_check(zmq_msg_init_size(&msg, size));
        }

        void rebuild(const void *src, size_t size) {
            zmq_check(zmq_msg_close(&msg));
            zmq_check(zmq_msg_init_size(&msg, size));
            memcpy(data(), src, size);
        }

        void rebuild(void *src, size_t size, free_fn *ffn, void *hint = nullptr) {
            zmq_check(zmq_msg_close(&msg));
            zmq_check(zmq_msg_init_data(&msg, src, size, ffn, hint));
        }

        void move(const message *m) {
            zmq_check(zmq_msg_move(&msg, const_cast<zmq_msg_t *>(&(m->msg))));
        }

        void copy(const message *m) {
            zmq_check(zmq_msg_copy(&msg, const_cast<zmq_msg_t *>(&(m->msg))));
        }

        bool more() const noexcept {
            return zmq_msg_more(const_cast<zmq_msg_t *>(&msg)) != 0;
        }

        void *data() noexcept { return zmq_msg_data(&msg); }

        const void *data() const noexcept {
            return zmq_msg_data(const_cast<zmq_msg_t *>(&msg));
        }

        size_t size() const noexcept {
            return zmq_msg_size(const_cast<zmq_msg_t *>(&msg));
        }

        template <typename T> T *data() noexcept {
            return static_cast<T *>(data());
        }

        template <typename T> T const *data() const noexcept {
            return static_cast<T const *>(data());
        }

    protected:
        message(const message &) = delete;
        void operator=(const message &) = delete;

    protected:
        zmq_msg_t msg;
    };

    class context {
        friend class socket;

    public:
        context() : ptr(zmq_ctx_new()) {
            if (!ptr)
                throw exception();
        }

        explicit context(int threads, int max_sockets = ZMQ_MAX_SOCKETS_DFLT) : ptr(zmq_ctx_new()) {
            if (!ptr)
                throw exception();
            zmq_check(zmq_ctx_set(ptr, ZMQ_IO_THREADS, threads));
            zmq_check(zmq_ctx_set(ptr, ZMQ_MAX_SOCKETS, max_sockets));
        }

        context(context &&rhs) noexcept : ptr(rhs.ptr) {
            rhs.ptr = nullptr;
        }

        context &operator=(context &&rhs) noexcept {
            std::swap(ptr, rhs.ptr);
            return *this;
        }

        ~context() noexcept {
            if (!ptr)
                return;
            int rc = zmq_ctx_destroy(ptr);
            assert(rc == 0);
        }

        void close() noexcept {
            zmq_check(zmq_ctx_shutdown(ptr));
            ptr = nullptr;
        }

    protected:
        context(const context &) = delete;
        void operator=(const context &) = delete;

    protected:
        void *ptr;
    };

    class socket {
    public:
        enum type {
            req = ZMQ_REQ,
            rep = ZMQ_REP,
            dealer = ZMQ_DEALER,
            router = ZMQ_ROUTER,
            pub = ZMQ_PUB,
            sub = ZMQ_SUB,
            xpub = ZMQ_XPUB,
            xsub = ZMQ_XSUB,
            push = ZMQ_PUSH,
            pull = ZMQ_PULL,
            pair = ZMQ_PAIR
        };

        socket() : ptr(nullptr) { }

        socket(context &context, type type) {
            init(context, (int) type);
        }

        socket(socket &&rhs) noexcept : ptr(rhs.ptr) {
            rhs.ptr = nullptr;
        }

        socket &operator=(socket &&rhs) noexcept {
            std::swap(ptr, rhs.ptr);
            return *this;
        }

        ~socket() noexcept {
            if (!ptr)
                return;
            int rv = zmq_close(ptr);
            assert(rv == 0);
        }

        explicit operator void *() noexcept { return ptr; }

        explicit operator const void *() const noexcept { return ptr; }

        void close() noexcept {
            if (!ptr)
                return;
            zmq_check(zmq_close(ptr));
            ptr = nullptr;
        }

        void setsockopt(int option, const void *value, size_t size) {
            zmq_check(zmq_setsockopt(ptr, option, value, size));
        }

        void getsockopt(int option, void *value, size_t *size) const {
            zmq_check(zmq_getsockopt(ptr, option, value, size));
        }

        template <typename T> void setsockopt(int option, T const &value) {
            setsockopt(option, &value, sizeof(T));
        }

        template <typename T> T getsockopt(int option) const {
            T value;
            size_t optlen = sizeof(T);
            getsockopt(option, &value, &optlen);
            return value;
        }

        void bind(const char *addr) {
            zmq_check(zmq_bind(ptr, addr));
        }

        void unbind(const char *addr_) {
            zmq_check(zmq_unbind(ptr, addr_));
        }

        void connect(const char *addr) {
            zmq_check(zmq_connect(ptr, addr));
        }

        void disconnect(const char *addr_) {
            zmq_check(zmq_disconnect(ptr, addr_));
        }

        void bind(const std::string &addr) { bind(addr.c_str()); }
        void unbind(std::string const &addr) { unbind(addr.c_str()); }
        void connect(const std::string &addr) { connect(addr.c_str()); }
        void disconnect(const std::string &addr) { disconnect(addr.c_str()); }

        bool connected() const noexcept { return ptr != nullptr; }
        bool more() const { return (bool) getsockopt<int>(ZMQ_RCVMORE); }

        bool send(const void *buf, size_t len, int flags = 0) {
            int nbytes = zmq_send(ptr, buf, len, flags);
            if (nbytes >= 0)
                return true;
            if (zmq_errno() == EAGAIN)
                return false;
            throw exception();
        }

        bool sendmore(const void *buf, size_t len, int flags = 0) {
            return send(buf, len, flags | ZMQ_SNDMORE);
        }

        bool send(int flags = 0) {
            return send(nullptr, 0, flags);
        }

        bool sendmore(int flags = 0) {
            return sendmore(nullptr, 0, flags);
        }

        bool send(message &m, int flags = 0) {
            int nbytes = zmq_msg_send(&m.msg, ptr, flags);
            if (nbytes >= 0)
                return true;
            if (zmq_errno() == EAGAIN)
                return false;
            throw exception();
        }

        bool sendmore(message &m, int flags = 0) {
            return send(m, flags | ZMQ_SNDMORE);
        }

        bool send(message &&m, int flags = 0) {
            return send(m, flags);
        }

        bool sendmore(message &&m, int flags = 0) {
            return sendmore(std::move(m), flags);
        }

        bool send(const std::string &m, int flags = 0) {
            return send(m.c_str(), m.length(), flags);
        }

        bool sendmore(const std::string &m, int flags = 0) {
            return sendmore(m.c_str(), m.length(), flags);
        }

        inline void send(const envelope &e);
        inline void sendmore(const envelope &e);

        template <typename T, typename = typename std::enable_if<std::is_trivially_copyable_v<T>>::type>
        bool send(const T &value, int flags = 0) {
            return send(&value, sizeof(T), flags);
        }

        template <typename T, typename = typename std::enable_if<std::is_trivially_copyable_v<T>>::type>
        bool sendmore(const T &value, int flags = 0) {
            return sendmore(&value, sizeof(T), flags);
        }

        bool recv(void *buf, size_t len, int flags = 0) {
            int nbytes = zmq_recv(ptr, buf, len, flags);
            if (nbytes >= 0) {
                if (nbytes != len)
                    throw exception("Message has an incorrect size (expected " +
                                    std::to_string(len) + " bytes, got " +
                                    std::to_string(nbytes) + " bytes)");
                return true;
            }
            if (zmq_errno() == EAGAIN)
                return false;
            throw exception();
        }

        bool recvmore(void *buf, size_t len, int flags = 0) {
            if (!recv(buf, len, flags))
                return false;
            if (!more())
                throw exception("Expected additional parts in multipart message");
            return true;
        }

        bool recv(int flags = 0) {
            return recv(nullptr, 0, flags);
        }

        bool recvmore(int flags = 0) {
            return recvmore(nullptr, 0, flags);
        }

        bool recv(message &m, int flags = 0) {
            int nbytes = zmq_msg_recv(&m.msg, ptr, flags);
            if (nbytes >= 0)
                return true;
            if (zmq_errno() == EAGAIN)
                return false;
            throw exception();
        }

        bool recvmore(message &m, int flags = 0) {
            if (!recv(m, flags))
                return false;
            if (!more())
                throw exception("Expected additional parts in multipart message");
            return true;
        }

        bool recv(std::string &s, int flags = 0) {
            message msg;
            if (!recv(msg, flags))
                return false;
            s = std::string((const char *) msg.data(), msg.size());
            return true;
        }

        bool recvmore(std::string &s, int flags = 0) {
            message msg;
            if (!recvmore(msg, flags))
                return false;
            s = std::string((const char *) msg.data(), msg.size());
            return true;
        }

        inline void recv(envelope &e);
        inline void recvmore(envelope &e);

        template <typename T, typename = typename std::enable_if<std::is_trivially_copyable_v<T>>::type>
        bool recv(T &value, int flags = 0) {
            return recv(&value, sizeof(T), flags);
        }

        template <typename T, typename = typename std::enable_if<std::is_trivially_copyable_v<T>>::type>
        bool recvmore(T &value, int flags = 0) {
            return recvmore(&value, sizeof(T), flags);
        }

        /// Gobble up the rest of a (partial) message and throw an exception
        void discard_remainder() {
            zmq::message temp;
            while (more())
                recv(temp);
        }

    protected:
        void init(context &context, int type) {
            ptr = zmq_socket(context.ptr, type);
            if (!ptr)
                throw exception();
        }

        socket(const socket &) = delete;
        void operator=(const socket &) = delete;

    protected:
        void *ptr;
    };

    typedef zmq_pollitem_t pollitem;

    enum poll_flags : short {
        pollin = ZMQ_POLLIN,
        pollout = ZMQ_POLLOUT
    };

    inline int poll(pollitem *items, size_t nitems, long timeout = -1) {
        int rc = zmq_poll(items, (int) nitems, timeout);
        if (rc < 0)
            throw exception();
        return rc;
    }

    inline int poll(pollitem *items, size_t nitems,
                    std::chrono::milliseconds timeout) {
        return poll(items, nitems, timeout.count());
    }

    inline int poll(std::vector<pollitem> &items,
                    std::chrono::milliseconds timeout) {
        return poll(items.data(), items.size(), timeout.count());
    }

    inline int poll(std::vector<pollitem> &items, long timeout = -1) {
        return poll(items.data(), items.size(), timeout);
    }

    inline std::tuple<int, int, int> version() {
        std::tuple<int, int, int> v;
        zmq_version(&std::get<0>(v), &std::get<1>(v), &std::get<2>(v));
        return v;
    }

    class envelope : public std::vector<std::string> {
    public:
        envelope() { }

        friend std::ostream& operator<<(std::ostream &os, const envelope &e) {
            os << '[';
            for (int i = 0; i < (int) e.size() - 1; ++i) {
                for (auto c : e[i])
                    os << std::setw(2) << std::setfill('0') << std::hex << (int) c;
            }
            os << ']';
            return os;
        }
    };

    inline void socket::send(const envelope &e) {
        for (size_t i = 0; i < e.size(); ++i)
            send(e[i], (i + 1 < e.size()) ? ZMQ_SNDMORE : 0);
    }

    inline void socket::sendmore(const envelope &e) {
        for (auto const &str : e)
            sendmore(str);
    }

    inline void socket::recv(envelope &e) {
        std::string str;
        e.clear();
        do {
            recv(str);
            e.push_back(std::move(str));
            if (e.back().empty())
                break;
            if (!more())
                throw exception("Expected additional parts in multipart message");
        } while (true);
    }

    inline void socket::recvmore(envelope &e) {
        recv(e);
        if (!more())
            throw exception("Expected additional parts in multipart message");
    }

    inline void catch_shutdown(void (*handler)(int)) {
        struct sigaction action;
        action.sa_handler = handler;
        action.sa_flags = 0;
        sigemptyset (&action.sa_mask);
        sigaction(SIGINT, &action, NULL);
        sigaction(SIGTERM, &action, NULL);
    }

    inline void dump (zmq::socket &socket) {
        std::cout << "----------------------------------------" << std::endl;

        while (true) {
            //  Process all parts of the message
            zmq::message message;
            socket.recv(message);

            //  Dump the message as text or binary
            int size = message.size();
            std::string data(static_cast<char*>(message.data()), size);

            bool is_text = true;
            int char_nbr;
            unsigned char byte;

            for (char_nbr = 0; char_nbr < size; char_nbr++) {
                byte = data [char_nbr];
                if (byte < 32 || byte > 127)
                    is_text = false;
            }
            std::cout << "[" << std::setfill('0') << std::setw(3) << size << "] ";
            if (!is_text && size > 0)
                std::cout << "0x";
            for (char_nbr = 0; char_nbr < size; char_nbr++) {
                if (is_text)
                    std::cout << (char)data [char_nbr];
                else
                    std::cout << std::setfill('0') << std::setw(2)
                       << std::hex << (unsigned int) data [char_nbr];
            }
            std::cout << std::endl;

            if (!message.more())
                break;              //  Last message part
        }
    }
}

namespace std {
    /// It's useful to be able to use envelopes as a hash table key. Add a partial
    /// overload to STL (this is allowed by the standard)
    template<>
    struct hash<zmq::envelope> {
        size_t operator()(const zmq::envelope &e) const {
            size_t value = 0;
            for (auto const &s : e)
                value ^= std::hash<std::string>()(s) + 0x9e3779b9 + (value << 6) + (value >> 2);
            return value;
        }
    };
};
