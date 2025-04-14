(*
 * The MIT License
 *
 * Copyright (c) 2024, Tarides
 * Author(s): Fabrice Buoro <fabrice@tarides.com>
 * Inspired by mirage-net-solo5 and mirage-net-unix
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *)

type netif_ptr = int
type netbuf_ptr = int

external uk_netdev_init : int -> (netif_ptr, string) result = "uk_netdev_init"
external uk_netdev_stop : netif_ptr -> unit = "uk_netdev_stop"
external uk_netdev_mac : netif_ptr -> string = "uk_netdev_mac"
external uk_netdev_mtu : netif_ptr -> int = "uk_netdev_mtu" [@@noalloc]

type bytes_array =
  (char, Bigarray.int8_unsigned_elt, Bigarray.c_layout) Bigarray.Array1.t

external uk_get_tx_buffer : netif_ptr -> int -> (netbuf_ptr, string) result
  = "uk_get_tx_buffer"

external uk_bigarray_of_netbuf : netbuf_ptr -> bytes_array
  = "uk_bigarray_of_netbuf"

external uk_netdev_tx : netif_ptr -> netbuf_ptr -> int -> (unit, string) result
  = "uk_netdev_tx"

external uk_netdev_rx :
    netif_ptr -> Cstruct.buffer -> int -> (int, string) result = "uk_netdev_rx"

open Lwt.Infix

let src = Logs.Src.create "netif" ~doc:"Mirage Unikraft network module"

module Log = (val Logs.src_log src : Logs.LOG)

type t = {
  id : int;
  mutable active : bool;
  netif : netif_ptr;
  mtu : int;
  stats : Mirage_net.stats;
  metrics :
    (int -> Metrics.field list, Mirage_net.stats -> Metrics.data) Metrics.src;
}

type error =
  [ Mirage_net.Net.error
  | `Allocation_error
  | `Invalid_length
  | `Unspecified_error
  | `Disconnected ]

let pp_error ppf = function
  | #Mirage_net.Net.error as e -> Mirage_net.Net.pp_error ppf e
  | `Allocation_error -> Fmt.string ppf "Allocation error"
  | `Unspecified_error -> Fmt.string ppf "Unspecified error"

let net_metrics () =
  let open Metrics in
  let doc = "Network statistics" in
  let data stat =
    Data.v
      [
        uint64 "received bytes" stat.Mirage_net.rx_bytes;
        uint32 "received packets" stat.rx_pkts;
        uint64 "transmitted bytes" stat.tx_bytes;
        uint32 "transmitted packets" stat.tx_pkts;
      ]
  in
  let tag = Tags.int "interface" in
  Src.v ~doc ~tags:Tags.[ tag ] ~data "net-unikraft"

let connect devid =
  let aux id =
    match uk_netdev_init id with
    | Ok netif ->
        let mtu = uk_netdev_mtu netif in
        let stats = Mirage_net.Stats.create () in
        let metrics = net_metrics () in
        let t = { id; active = true; netif; mtu; stats; metrics } in
        Lwt.return t
    | Error msg ->
        Log.err (fun f -> f "connect(%d): %s" id msg);
        Lwt.fail_with msg
  in
  match int_of_string_opt devid with
  | Some id when id >= 0 && id < 63 ->
      Log.info (fun f -> f "Plugging into %d" id);
      aux id
  | _ ->
      Lwt.fail_with
        (Fmt.str "connect(%s): net ids should be integers on this platform"
           devid)

let disconnect t =
  Log.info (fun f -> f "Disconnect %d" t.id);
  t.active <- false;
  uk_netdev_stop t.netif;
  Lwt.return_unit

let write_pure t ~size fill =
  match uk_get_tx_buffer t.netif size with
  | Error _ -> Error `Allocation_error
  | Ok netbuf -> (
      let ba = uk_bigarray_of_netbuf netbuf in
      let buf = Cstruct.of_bigarray ~len:size ba in
      let len = fill buf in
      if len > size then Error `Invalid_length
      else
        match uk_netdev_tx t.netif netbuf len with
        | Error msg ->
            Log.err (fun f -> f "Sending packet: %s" msg);
            Error `Unspecified_error
        | Ok _ ->
            Mirage_net.Stats.tx t.stats (Int64.of_int len);
            Metrics.add t.metrics (fun x -> x t.id) (fun d -> d t.stats);
            Ok ())

let write t ~size fill =
  if t.active then Lwt.return (write_pure t ~size fill)
  else Lwt.return (Error `Disconnected)

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
      | Ok 0 -> Lwt.return (Error `Continue)
      | Ok size ->
          Mirage_net.Stats.rx t.stats (Int64.of_int size);
          Metrics.add t.metrics (fun x -> x t.id) (fun d -> d t.stats);
          let buf = Cstruct.sub buf 0 size in
          Lwt.return (Ok buf)
      | Error msg ->
          Log.err (fun f -> f "Error receiving: %s" msg);
          Lwt.return (Error `Unspecified_error)
  in
  process () >>= function
  | Error `Continue ->
      Unikraft_os.Main.UkEngine.wait_for_work_netdev t.id >>= fun () ->
      read t buf
  | Error `Canceled -> Lwt.return (Error `Canceled)
  | Error `Unspecified_error -> Lwt.return (Error `Disconnected)
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
  | false -> Lwt.return (Error `Disconnected)
