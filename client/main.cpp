#include "spawn.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <deque>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <type_traits>
using boost::asio::ip::tcp;
template<typename... Ts>
std::string format(Ts&&... args)
{
    std::ostringstream oss;
    (oss <<...<< std::forward<Ts>(args));
    return oss.str();
}

template<typename T>
std::optional<T> from_chars(std::string_view sv) noexcept
{
    T out;
    auto end = sv.data()+sv.size();
    auto res = std::from_chars(sv.data(),end,out);
    if(res.ec==std::errc{}&&res.ptr==end)
        return out;
    return {};
}

class logger
{
public:
    template<typename T>
    logger& operator<<(T&& x)
    {
        std::cerr << std::forward<T>(x);
        return *this;
    }

    ~logger()
    {
        std::cerr << std::endl;
    }
private:
    static std::mutex m_;

    std::lock_guard<std::mutex> l_{m_};
};

std::mutex logger::m_;

using bigint = boost::multiprecision::cpp_int;

template<typename F>
auto at_scope_exit(F&& f)
{
    using f_t = std::remove_cvref_t<F>;
    static_assert(std::is_nothrow_destructible_v<f_t>&&
                  std::is_nothrow_invocable_v<f_t>);
    struct ase_t
    {
        F f;

        ase_t(F&& f)
            : f(std::forward<F>(f))
        {}

        ase_t(const ase_t&) = default;
        ase_t(ase_t&&) = delete;
        ase_t operator=(const ase_t&) = delete;
        ase_t operator=(ase_t&&) = delete;

        ~ase_t()
        {
            std::forward<F>(f)();
        }
    };
    return ase_t{std::forward<F>(f)};
}

int main() {

    try {
        auto port = from_chars<std::uint16_t>("3002");
        if (!port || !*port)
            throw std::runtime_error("Port must be in [1;65535]");
        boost::asio::io_context ctx;
        boost::asio::signal_set stop_signals{ctx, SIGINT, SIGTERM};
        stop_signals.async_wait([&](boost::system::error_code ec, int /*signal*/) {
            if (ec)
                return;
            logger{} << "Terminating in response to signal.";
            ctx.stop();
        });

        boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address::from_string ("127.0.0.1"), 3002);

        for (int cl=0; cl<10; cl++){
             boost::asio::ip::tcp::socket socket_new { make_strand (ctx) };

            bccoro::spawn (bind_executor (ctx,[socket = boost::beast::tcp_stream { std::move (socket_new) }, endpoint, port = *port]
                                                (bccoro::yield_context yc) mutable
             {
                boost::asio::ip::tcp::endpoint ep { boost::asio::ip::tcp::v4 (), port };
                constexpr static std::chrono::seconds timeout { 160 };
                boost::system::error_code ec;
                constexpr static std::size_t limit = 1024;
                std::string in_buf;

                for (;;) {

                    std::string ch="ls\n";
                    socket.async_connect(ep, yc[ec]);
                     socket.expires_after(timeout);
                    socket.async_write_some(boost::asio::buffer(ch, ch.length()), yc[ec]);
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted)
                            logger{} << "Fail in request: "
                                     << ec.message();
                        return;
                    }
                    socket.expires_after(timeout);
                    std::size_t n = async_read_until(
                        socket,boost::asio::dynamic_string_buffer(in_buf,limit),
                        '\n',yc[ec]);
                    logger{} << in_buf;
                    if (ec) {
                        if (ec != boost::asio::error::operation_aborted)
                            logger{} << "Read Error: " << ec.message();
                        return;
                    }
                }


        }));
        }
        std::vector<std::thread> workers;
        size_t extra_workers = std::thread::hardware_concurrency() - 1;
        workers.reserve(extra_workers);
        auto ase = at_scope_exit([&]() noexcept {
            for (auto &t:workers)
                t.join();
        });
        for (size_t i = 0; i < extra_workers; ++i)
            workers.emplace_back([&] {
                ctx.run();
            });
        ctx.run();
    }
    catch (...) {
        logger{} << boost::current_exception_diagnostic_information();
        return 1;
    }
}

