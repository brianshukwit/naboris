module Req = Req;
module Res = Res;
module Router = Router;

open Lwt.Infix;

let buildConnectionHandler = (serverConfig: ServerConfig.t('sessionData)) => {
  let request_handler =
      (_client_address: Unix.sockaddr, request_descriptor: Httpaf.Reqd.t) => {
    let request: Httpaf.Request.t = Httpaf.Reqd.request(request_descriptor);
    let target = request.target;
    let meth = Method.ofHttpAfMethod(request.meth);
    let route = Router.generateRoute(target, meth);

    Lwt.async(() => {
      let rawReq =
        Req.fromReqd(request_descriptor, serverConfig.sessionConfig);

      SessionManager.resumeSession(serverConfig, rawReq)
      >>= (req => switch(serverConfig.middlewares) {
        | [] => serverConfig.routeRequest(route, req, Res.default())
        | [oneMiddleware] => oneMiddleware(serverConfig.routeRequest, route, req, Res.default());
        | _moreThanOneMiddleware =>
          let fullHandler = serverConfig.middlewares
            |> List.rev
            |> List.fold_left((next: RequestHandler.t('a), current) => current(next), serverConfig.routeRequest)
          
          fullHandler(route, req, Res.default());
      });
    });
  };

  let error_handler =
      (
        _client_address: Unix.sockaddr,
        ~request as _=?,
        _error,
        start_response,
      ) => {
    let response_body = start_response(Httpaf.Headers.empty);
    Httpaf.Body.write_string(response_body, "Unknown Error");
    Httpaf.Body.close_writer(response_body);
  };

  Httpaf_lwt_unix.Server.create_connection_handler(
    ~config=?ServerConfig.toHttpAfConfig(serverConfig),
    ~request_handler,
    ~error_handler,
  );
};