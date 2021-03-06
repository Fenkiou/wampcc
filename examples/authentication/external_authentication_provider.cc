/*
 * Copyright (c) 2017 Darren Smith
 * Copyright (c) 2018 Daniel Kesler
 *
 * wampcc is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "wampcc/wampcc.h"

#include <iostream>

using namespace wampcc;
using namespace std;

int main(int, char**)
{
  try {
    /* Create the wampcc kernel. */

    kernel the_kernel({}, logger::console());

    /* Create an embedded wamp router. */

    wamp_router router(&the_kernel);

    /* Accept clients on IPv4 port, without authentication. */

    auth_provider auth = {
      // provider_name
      [](const std::string& realm) {
        return "ticket_auth";
      },
      // policy
      [](const std::string& user, const std::string& realm) {
        if(realm == "default_realm")
          return auth_provider::auth_plan{ auth_provider::mode::open, {} };
        else if(realm == "private_realm")
          return auth_provider::auth_plan(auth_provider::mode::authenticate, {"ticket"});
        else
          return auth_provider::auth_plan(auth_provider::mode::forbidden, {});
      },
      // cra_salt
      nullptr,
      // check_cra
      nullptr,
      // user_secret
      nullptr,
      // check_ticket
      nullptr,
      // user_role,
      nullptr,
      // authorize
      nullptr,
      // authenticate
      [](const std::string& user, const std::string& realm, const std::string& authmethod, const std::string& signiture) {
        auth_provider::authenticated auth;
        auth.allow = true;
        auth.role = WAMP_ANONYMOUS;
        auth.authid = user;

        std::cout << "authenticate (user: " << user
                  << ", realm: " << realm
                  << ", method: " << authmethod
                  << ", signiture: " << signiture << std::endl;

        return auth;
      }
    };

    auto fut = router.listen(auth, 55555);

    if (auto ec = fut.get())
      throw runtime_error(ec.message());

    /* Provide an RPC named 'greeting' on realm 'default_realm'. */

    router.callable("default_realm", "greeting",
                    [](wamp_router&, wamp_session& caller, call_info info) {
      caller.result(info.request_id, {"hello"});
    });

    router.callable("private_realm", "greeting",
                    [](wamp_router&, wamp_session& caller, call_info info) {
      caller.result(info.request_id, {"hello private member"});
    });

    /* Suspend main thread */
    std::promise<void> forever;
    forever.get_future().wait();
  } catch (const exception& e) {
    cout << e.what() << endl;
    return 1;
  }
}
