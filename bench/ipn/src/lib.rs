// SPDX-License-Identifier: MIT
//! Обёртка для C: ipnsort из исходников rivals/ipnsort (L. Bergdoll, O. Peters) и, для сверки,
//! slice::sort_unstable стандартной библиотеки Rust (с версии 1.81 — тот же ipnsort).
use std::cell::Cell;
#[no_mangle]
pub extern "C" fn ipn_sort_u64(p: *mut u64, n: usize) {
    let s = unsafe { std::slice::from_raw_parts_mut(p, n) };
    ipnsort::sort(s);
}
#[no_mangle]
pub extern "C" fn ipn_sort_u64_cnt(p: *mut u64, n: usize, cnt: *mut i64) {
    let s = unsafe { std::slice::from_raw_parts_mut(p, n) };
    let c = Cell::new(0i64);
    ipnsort::sort_by(s, |a, b| { c.set(c.get() + 1); a.cmp(b) });
    unsafe { *cnt = c.get(); }
}
#[no_mangle]
pub extern "C" fn ipnstd_sort_u64(p: *mut u64, n: usize) {
    let s = unsafe { std::slice::from_raw_parts_mut(p, n) };
    s.sort_unstable();
}
#[no_mangle]
pub extern "C" fn ipnstd_sort_u64_cnt(p: *mut u64, n: usize, cnt: *mut i64) {
    let s = unsafe { std::slice::from_raw_parts_mut(p, n) };
    let c = Cell::new(0i64);
    s.sort_unstable_by(|a, b| { c.set(c.get() + 1); a.cmp(b) });
    unsafe { *cnt = c.get(); }
}
