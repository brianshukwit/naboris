open Lwt.Infix;

type t = {
  status: int,
  headers: list((string, string)),
  closed: bool,
  exn: option(exn),
};

let default = () => {status: 200, headers: [], closed: false, exn: None};

let createResponse = (res: t) => {
  Httpaf.Response.create(
    ~headers=Httpaf.Headers.of_list(res.headers),
    Httpaf.Status.of_code(res.status),
  );
};

let addHeader = (header: (string, string), res: t) => {
  let (key, value) = header;
  {...res, headers: [(String.lowercase_ascii(key), value), ...res.headers]};
};

let addHeaderIfNone = (header, res) => {
  let (key, _) = header;
  let existing = List.fold_left((exists, h) => switch(exists) {
    | true => true
    | false =>
      let (k, _) = h;
      k == key;
  }, false, res.headers);

  switch(existing) {
    | true => res
    | false => addHeader(header, res);
  };
};

let status = (status: int, res: t) => {
  {...res, status};
};

let closeResponse = (res) => { ...res, closed: true };

let raw = (req: Req.t('a), body: string, res: t) => {
  let resWithHeaders = addHeaderIfNone(("Content-length", String.length(body) |> string_of_int), res)
    |> addHeaderIfNone(("Connection", "keep-alive"));
  let response = createResponse(resWithHeaders);
  let requestDescriptor = Req.reqd(req);

  let responseBody =
    Httpaf.Reqd.respond_with_streaming(requestDescriptor, response);
  Httpaf.Body.write_string(responseBody, body);
  Httpaf.Body.close_writer(responseBody);
  Lwt.return(closeResponse(resWithHeaders));
};

let text = (req: Req.t('a), body: string, res: t) => addHeader(("Content-Type", "text/plain"), res)
  |> raw(req, body);

let json = (req: Req.t('a), body: string, res: t) => addHeader(("Content-Type", "application/json"), res)
  |> raw(req, body);

let html = (req: Req.t('a), htmlBody: string, res: t) => addHeader(("Content-Type", "text/html"), res)
  |> raw(req, htmlBody);

let streamFileContentsToBody = (fullFilePath, responseBody) => {
  let readOnlyFlags = [Unix.O_RDONLY];
  let readOnlyPerm = 444;
  Lwt_unix.openfile(fullFilePath, readOnlyFlags, readOnlyPerm) >>= ((fd) => {
    let channel = Lwt_io.of_fd(fd, ~mode=Lwt_io.Input);
    let bufferSize = Lwt_io.default_buffer_size();
    let rec pipeBody = (~count, ch, body) =>
      Lwt.bind(
        Lwt_io.read(~count, ch),
        chunk => {
          Httpaf.Body.write_string(body, chunk);
          String.length(chunk) < count
            ? {
              Lwt.return_unit;
            }
            : pipeBody(~count, ch, body);
        },
      );
    Lwt.finalize(
      () => pipeBody(~count=bufferSize, channel, responseBody),
      () => {
        Httpaf.Body.close_writer(responseBody);
        Lwt_io.close(channel);
      },
    );
  });
};

let static = (basePath, pathList, req: Req.t('a), res) => {
  let fullFilePath = Static.getFilePath(basePath, pathList);
  Lwt_unix.file_exists(fullFilePath) >>= ((exists) => switch (exists) {
    | true =>
      Lwt_unix.stat(fullFilePath) >>= ((stats) => {
        let size = stats.st_size;
        let resWithHeaders =
          addHeader(("Content-Type", MimeTypes.getMimeType(fullFilePath)), res)
          |> addHeader(("Content-Length", string_of_int(size)));
        let response = createResponse(resWithHeaders);
        let requestDescriptor = Req.reqd(req);
        let responseBody =
          Httpaf.Reqd.respond_with_streaming(requestDescriptor, response);
        streamFileContentsToBody(fullFilePath, responseBody)
          >>= (() => Lwt.return @@ closeResponse @@ resWithHeaders);
      });
    | _ =>
      let resWithHeaders =
        status(404, res) |> addHeader(("Content-Length", "9")) |> addHeader(("Connection", "keep-alive"));
      let response = createResponse(resWithHeaders);
      let responseBody =
        Httpaf.Reqd.respond_with_streaming(Req.reqd(req), response);
      Httpaf.Body.write_string(responseBody, "Not found");
      Httpaf.Body.close_writer(responseBody);
      Lwt.return @@ closeResponse @@ resWithHeaders;
    });
};

let setSessionCookies = (newSessionId, sessionIdKey, maxAge, res) => {
  let setCookieKey = "Set-Cookie";
  let maxAgeStr = string_of_int(maxAge);

  addHeader(
    (
      setCookieKey,
      sessionIdKey ++ "=" ++ newSessionId ++ "; Max-Age=" ++ maxAgeStr ++ ";",
    ),
    res,
  );
};

let redirect = (path, req, res) => {
  status(302, res) |> addHeader(("Location", path)) |> text(req, "Found");
};

let reportError = (req: Req.t('a), res, exn) => {
  Httpaf.Reqd.report_exn(Req.reqd(req), exn);
  Lwt.return @@ closeResponse({...res, exn: Some(exn)});
};

let writeChannel = (req: Req.t('a), res) => {
  let reqd = Req.reqd(req);
  let resWithHeaders = addHeader(("Transfer-encoding", "chunked"), res)
    |> addHeader(("Connection", "keep-alive"));

  let responseBody = resWithHeaders
    |> createResponse
    |> Httpaf.Reqd.respond_with_streaming(~flush_headers_immediately=true, reqd);

  let onWrite = (bytes, off, len) => {
    Httpaf.Body.write_bigstring(~off, ~len, responseBody, bytes);
    Lwt.return(len);
  };

  let (respPromise, respResolver) = Lwt.task();

  let close = () => {
    Httpaf.Body.close_writer(responseBody);
    Lwt.wakeup(respResolver, closeResponse(resWithHeaders));
    Lwt.return_unit;
  };

  (Lwt_io.make(
    ~close,
    ~mode=Output,
    onWrite
  ), respPromise);
};