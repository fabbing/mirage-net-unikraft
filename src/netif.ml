(*
 * FIXME previous authors
 * Copyright (c) 2024 Fabrice Buoro <fabrice@tarides.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *)

external uk_netdev_init : int -> (int64, string) result = "uk_netdev_init"
external uk_netdev_stop : int64 -> unit = "uk_netdev_stop"

external uk_netdev_mac : int64 -> string = "uk_netdev_mac"
external uk_netdev_mtu : int64 -> int = "uk_netdev_mtu" [@@noalloc]

type bytes_array =
  (char, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

external uk_get_tx_buffer : int64 -> int -> bool * int64 * string
  = "uk_get_tx_buffer"

external uk_ba_of_netbuf : int64 -> bytes_array = "uk_ba_of_netbuf"
external uk_netdev_tx : int64 -> int64 -> int -> bool * string = "uk_netdev_tx"

external uk_netdev_rx : int64 -> Cstruct.buffer -> int -> bool * int * string
  = "uk_netdev_rx"

open Lwt.Infix

let src = Logs.Src.create "netif" ~doc:"Mirage Unikraft network module"

module Log = (val Logs.src_log src : Logs.LOG)

type t = {
  id : int;
  mutable active : bool;
  netif : int64;
  mtu : int;
  stats : Mirage_net.stats;
      (*
  metrics :
    (string -> Metrics.field list, Mirage_net.stats -> Metrics.data) Metrics.src;
  *)
}

type error =
  [ Mirage_net.Net.error
  | `Invalid_argument
  | `Unspecified_error
  | `Disconnected ]

let pp_error ppf = function
  | #Mirage_net.Net.error as e -> Mirage_net.Net.pp_error ppf e
  | `Invalid_argument -> Fmt.string ppf "Invalid argument"
  | `Unspecified_error -> Fmt.string ppf "Unspecified error"
  | `Disconnected -> Fmt.string ppf "Disconnected"

let connect devid =
  let aux id =
    match uk_netdev_init id with
    | Ok netif ->
        let mtu = uk_netdev_mtu netif in
        let stats = Mirage_net.Stats.create () in
        let t = { id; active = true; netif; mtu; stats } in
        Lwt.return t
    | Error err -> Lwt.fail_with err
  in
  let id = try Some (int_of_string devid) with _ -> None in
  match id with
  | Some id when id >= 0 ->
      Log.info (fun f -> f "Plugging into %d" id);
      aux id
  | None -> Lwt.fail_with (Fmt.str "Netif: connect(%s): Invalid argument" devid)

let disconnect t =
  Log.info (fun f -> f "Disconnect %d" t.id);
  t.active <- false;
  uk_netdev_stop t.netif;
  Lwt.return_unit

let write_pure t ~size fill =
  match uk_get_tx_buffer t.netif size with
  | false, _, err -> Error `Unspecified_error
  | true, netbuf, _ -> (
      let ba = uk_ba_of_netbuf netbuf in
      let buf = Cstruct.of_bigarray ~len:size ba in
      let len = fill buf in
      if len > size then Error `Invalid_length
      else
        match uk_netdev_tx t.netif netbuf len with
        | true, _ ->
            Mirage_net.Stats.tx t.stats (Int64.of_int len);
            (*Metrics.add t.metrics (fun x -> x t.id) (fun d -> d t.stats);*)
            Ok ()
        | false, err -> Error `Unspecified_error (* FIXME forward error *))

let write t ~size fill = Lwt.return (write_pure t ~size fill)
let mac t = Macaddr.of_octets_exn (uk_netdev_mac t.netif)
let mtu t = t.mtu
let get_stats_counters t = t.stats
let reset_stats_counters t = Mirage_net.Stats.reset t.stats

(* Input a frame, and block if nothing is available *)
let rec read t buf =
  let process () =
    if not (Unikraft_os.Main.UkEngine.data_on_netdev t.id) then
      Lwt.return (Error `Continue)
    else
      (* TODO what about offset? *)
      match uk_netdev_rx t.netif buf.Cstruct.buffer buf.Cstruct.len with
      | true, 0, _ -> Lwt.return (Error `Continue)
      | true, size, _ ->
          Mirage_net.Stats.rx t.stats (Int64.of_int size);
          let buf = Cstruct.sub buf 0 size in
          Lwt.return (Ok buf)
      | false, _, err -> Lwt.return (Error `Disconnected)
  in
  process () >>= function
  | Error `Continue ->
      Unikraft_os.Main.UkEngine.wait_for_work_netdev t.id >>= fun () ->
      read t buf
  | Error `Canceled -> Lwt.return (Error `Canceled)
  | Error `Disconnected -> Lwt.return (Error `Disconnected)
  | Ok buf -> Lwt.return (Ok buf)

(* Loop and listen for packets permanently *)
(* this function has to be tail recursive, since it is called at the
   top level, otherwise memory of received packets and all reachable
   data is never claimed.  take care when modifying, here be dragons! *)
let rec listen t ~header_size fn =
  match t.active with
  | true -> (
      let buf = Cstruct.create (t.mtu + header_size) in
      let process () =
        read t buf >|= function
        | Ok buf ->
            Lwt.async (fun () -> fn buf);
            Ok ()
        | Error `Canceled -> Error `Disconnected
        | Error `Disconnected ->
            t.active <- false;
            Error `Disconnected
      in
      process () >>= function
      | Ok () -> (listen [@tailcall]) t ~header_size fn
      | Error e -> Lwt.return (Error e))
  | false -> Lwt.return (Ok ())
