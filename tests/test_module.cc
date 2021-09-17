#include "sylar/module.h"
#include "sylar/singleton.h"
#include <iostream>
#include "sylar/log.h"
#include "sylar/db/redis.h"
#include "sylar/grpc/grpc_stream.h"
#include "sylar/grpc/grpc_servlet.h"
#include "sylar/application.h"
#include "tests/test.pb.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

class A {
public:
    A() {
        std::cout << "A::A " << this << std::endl;
    }

    ~A() {
        std::cout << "A::~A " << this << std::endl;
    }

};


int32_t HandleTest(sylar::http::HttpRequest::ptr request
                   , sylar::http::HttpResponse::ptr response
                   , sylar::SocketStream::ptr session) {
    SYLAR_LOG_INFO(g_logger) << "request *** " << *request;
    response->setBody("hello test");
    response->setHeader("random", std::to_string(time(0)));
    return 0;
}

int32_t HandleHelloServiceHello(sylar::grpc::GrpcRequest::ptr request,
                                sylar::grpc::GrpcResult::ptr response,
                                sylar::SocketStream::ptr session) {
    SYLAR_LOG_INFO(g_logger) << *request->getRequest();
    auto req = request->getAsPB<test::HelloRequest>();
    if(!req) {
        response->setResult(100);
        response->setError("invalid pb");
        return -1;
    }
    SYLAR_LOG_INFO(g_logger) << "---" << sylar::PBToJsonString(*req) << " - " << req;

    test::HelloResponse rsp;
    //rsp.set_id("hello");
    //rsp.set_msg("world");
    response->setAsPB(rsp);
    return 0;
}

class MyModule : public sylar::RockModule {
public:
    MyModule()
        :RockModule("hello", "1.0", "") {
        //sylar::Singleton<A>::GetInstance();
    }

    bool onLoad() override {
        sylar::Singleton<A>::GetInstance();
        std::cout << "-----------onLoad------------" << std::endl;
        return true;
    }

    bool onUnload() override {
        sylar::Singleton<A>::GetInstance();
        std::cout << "-----------onUnload------------" << std::endl;
        return true;
    }

    bool onServerReady() {
        std::vector<sylar::TcpServer::ptr> svrs;
        sylar::Application::GetInstance()->getServer("http2", svrs);
        for(auto& i : svrs) {
            auto h2 = std::dynamic_pointer_cast<sylar::http2::Http2Server>(i);
            auto slt = h2->getServletDispatch();
            slt->addServlet("/test", HandleTest);

            slt->addServlet(sylar::grpc::GrpcServlet::GetGrpcPath("test", "HelloService", "Hello")
                    , sylar::grpc::GrpcFunctionServlet::Create(HandleHelloServiceHello));
        }
        registerService("rock", "sylar.top", "blog");
        auto rpy = sylar::RedisUtil::Cmd("local", "get abc");
        if(!rpy) {
            SYLAR_LOG_ERROR(g_logger) << "redis cmd get abc error";
        } else {
            SYLAR_LOG_ERROR(g_logger) << "redis get abc: "
                << (rpy->str ? rpy->str : "(null)");
        }

        sylar::IOManager::GetThis()->addTimer(1000, [](){
            auto lb = sylar::Application::GetInstance()->getGrpcSDLoadBalance();
            test::HelloRequest hr;
            hr.set_id("hello_" + std::to_string(time(0)));
            hr.set_msg("world_" + std::to_string(time(0)));
            auto rt = lb->request("grpc", "test", "/test.HelloService/Hello", hr, 1000);
            SYLAR_LOG_INFO(g_logger) << rt->toString();
            if(rt->getResponse()) {
                SYLAR_LOG_INFO(g_logger) << *rt->getResponse();
            }
        }, true);

        return true;
    }

    bool handleRockRequest(sylar::RockRequest::ptr request
                        ,sylar::RockResponse::ptr response
                        ,sylar::RockStream::ptr stream) {
        //SYLAR_LOG_INFO(g_logger) << "handleRockRequest " << request->toString();
        //sleep(1);
        response->setResult(0);
        response->setResultStr("ok");
        response->setBody("echo: " + request->getBody());

        usleep(100 * 1000);
        auto addr = stream->getLocalAddressString();
        if(addr.find("8061") != std::string::npos) {
            if(rand() % 100 < 50) {
                usleep(10 * 1000);
            } else if(rand() % 100 < 10) {
                response->setResult(-1000);
            }
        } else {
            //if(rand() % 100 < 25) {
            //    usleep(10 * 1000);
            //} else if(rand() % 100 < 10) {
            //    response->setResult(-1000);
            //}
        }
        return true;
        //return rand() % 100 < 90;
    }

    bool handleRockNotify(sylar::RockNotify::ptr notify 
                        ,sylar::RockStream::ptr stream) {
        SYLAR_LOG_INFO(g_logger) << "handleRockNotify " << notify->toString();
        return true;
    }

};

extern "C" {

sylar::Module* CreateModule() {
    sylar::Singleton<A>::GetInstance();
    std::cout << "=============CreateModule=================" << std::endl;
    return new MyModule;
}

void DestoryModule(sylar::Module* ptr) {
    std::cout << "=============DestoryModule=================" << std::endl;
    delete ptr;
}

}
