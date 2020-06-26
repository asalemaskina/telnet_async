#ifndef SPAWN_HPP
#define SPAWN_HPP
#ifndef UUID_0422FEF5_A50E_4618_AB6C_57BA2F417A2E
#define UUID_0422FEF5_A50E_4618_AB6C_57BA2F417A2E

#include <boost/asio/async_result.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/context/continuation.hpp>
#include <boost/system/system_error.hpp>

#include <memory>
#include <utility>
#include <optional>

namespace bccoro
{
struct yield_context;

namespace detail
{
struct coro_data
{
    boost::asio::executor ex_;
    boost::context::continuation cont_;

    coro_data(boost::asio::executor ex) noexcept
        : ex_{std::move(ex)}
    {}

    virtual ~coro_data() = default;
};
}

struct yield_context
{
    explicit yield_context(std::weak_ptr<detail::coro_data> data) noexcept
        : data_{std::move(data)}
    {}

    yield_context operator[](boost::system::error_code& ec) noexcept
    {
        yield_context yc{*this};
        yc.ec_ = &ec;
        return yc;
    }

    std::weak_ptr<detail::coro_data> data_;
    boost::system::error_code* ec_ = nullptr;
};

template<typename Function>
void spawn(Function f)
{
    struct coro_data_impl : detail::coro_data
    {
        // Type erasing with std::function would require copyable Function.
        Function f_;

        coro_data_impl(Function f)
            : detail::coro_data{boost::asio::get_associated_executor(f)},
            f_{std::move(f)}
        {}

        ~coro_data_impl() override
        {
            // Unwind coro stack before destroying the function object itself.
            cont_ = {};
        }
    };
    auto ex = boost::asio::get_associated_executor(f);
    boost::asio::post(boost::asio::bind_executor(ex,
                                                 [data=std::make_shared<coro_data_impl>(std::move(f))]{
                                                     data->cont_ = boost::context::callcc([&](boost::context::continuation&& cont){
                                                         data->cont_ = std::move(cont);
                                                         data->f_(yield_context{data});
                                                         return std::move(data->cont_);
                                                     });
                                                 }
                                                 ));
}
}

namespace boost::asio
{
template<>
class async_result<bccoro::yield_context,void (boost::system::error_code)>
{
public:
    using return_type = void;

    struct completion_handler_type
    {
        // TODO: propogate allocator from original function.

        using executor_type = boost::asio::executor;

        std::shared_ptr<bccoro::detail::coro_data> data_;
        boost::system::error_code* out_ec_;
        async_result* ar_;

        explicit completion_handler_type(bccoro::yield_context&& yc) noexcept
            : data_{yc.data_},
            out_ec_{yc.ec_}
        {}

        executor_type get_executor() const noexcept
        {
            return data_->ex_;
        }

        void operator()(boost::system::error_code ec)
        {
            ar_->ec_ = ec;
            ar_->out_ec_ = out_ec_;
            data_->cont_ = std::move(data_->cont_).resume();
        }
    };

    explicit async_result(completion_handler_type& h) noexcept
        : data_{h.data_.get()}
    {
        h.ar_ = this;
    }

    void get()
    {
        data_->cont_ = std::move(data_->cont_).resume();
        if(ec_){
            if(!out_ec_)
                throw boost::system::system_error{ec_};
            *out_ec_ = ec_;
        }
    }
private:
    bccoro::detail::coro_data* data_;
    boost::system::error_code ec_,*out_ec_;
};

template<typename Arg>
class async_result<bccoro::yield_context,void (boost::system::error_code,Arg)>
    : public async_result<bccoro::yield_context,void (boost::system::error_code)>
{
    using base_t = async_result<bccoro::yield_context,void (boost::system::error_code)>;
public:
    using return_type = Arg;

    struct completion_handler_type : base_t::completion_handler_type
    {
        using base_t::completion_handler_type::completion_handler_type;

        void operator()(boost::system::error_code ec,Arg a)
        {
            static_cast<async_result*>(this->ar_)->res_ = std::move(a);
            base_t::completion_handler_type::operator()(ec);
        }
    };

    using base_t::async_result;

    Arg get()
    {
        base_t::get();
        return std::move(*res_);
    }
private:
    std::optional<Arg> res_;
};
}

#endif

#endif // SPAWN_HPP
