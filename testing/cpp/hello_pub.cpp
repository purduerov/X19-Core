#include <publisher.hpp>
#include <telemetry.pb.h>

int main(){
    auto pub = CppMsg::Publisher<rov::telemetry::test>("tcp://*:5556", "test");
    for(int i = 0; i < 10; i++){
        rov::telemetry::test msg;
        msg.set_msg("Hi");
        pub.publish(msg);
    }

    return 0;
}