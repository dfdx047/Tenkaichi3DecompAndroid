#include "ee/retained_ee_stack.h"
#include <iostream>
#include <stdexcept>
static void require(bool value) { if (!value) throw std::runtime_error("Retained EE stack assertion failed"); }
struct Guard { int& count; ~Guard() { ++count; } };
int main() {
    try {
        int entries=0, destructors=0, result=0;
        RetainedEeStack* active=nullptr;
        RetainedEeStack stack([&] {
            ++entries; Guard guard{destructors}; int local=41;
            active->yield(); require(local==41); ++local;
            active->yield(); result=local;
        }); active=&stack;
        stack.resume(); require(entries==1 && destructors==0 && result==0 && stack.state()==RetainedEeStack::State::Waiting);
        stack.resume(); require(entries==1 && destructors==0 && result==0);
        stack.resume(); require(entries==1 && destructors==1 && result==42 && stack.state()==RetainedEeStack::State::Completed);
        bool rejected=false;try {stack.resume();}catch(const std::logic_error&){rejected=true;}require(rejected);
        {
            RetainedEeStack parked([&]{Guard guard{destructors};active->yield();result=99;});active=&parked;
            parked.resume();require(destructors==1);
        }
        require(destructors==2 && result==42);
        RetainedEeStack failed([&]{Guard guard{destructors};throw std::runtime_error("guest failure");});
        rejected=false;try{failed.resume();}catch(const std::runtime_error&){rejected=true;}
        require(rejected && destructors==3 && failed.state()==RetainedEeStack::State::Failed);
        RetainedEeStack* outer_ptr=nullptr;RetainedEeStack* inner_ptr=nullptr;
        RetainedEeStack inner([&]{inner_ptr->yield();result=7;});inner_ptr=&inner;
        RetainedEeStack outer([&]{inner.resume();require(result==42);outer_ptr->yield();inner.resume();});outer_ptr=&outer;
        outer.resume();require(result==42);outer.resume();require(result==7);
        std::cout << "Retained EE stacks: frame retention, resume, cancellation, failure and nested ownership verified.\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
