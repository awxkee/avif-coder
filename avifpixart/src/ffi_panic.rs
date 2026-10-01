use std::panic::{UnwindSafe, catch_unwind};

/// Convert a panic into an existing C error sentinel without unwinding into C++.
/// The caller must discard any output that was being constructed when it panicked.
pub(crate) fn catch_unwind_or<T>(fallback: T, operation: impl FnOnce() -> T + UnwindSafe) -> T {
    catch_unwind(operation).unwrap_or_else(|_payload| fallback)
}

#[cfg(test)]
mod tests {
    use super::catch_unwind_or;
    use std::sync::atomic::{AtomicBool, Ordering};

    #[test]
    fn successful_result_is_preserved() {
        assert_eq!(catch_unwind_or(0, || 42), 42);
    }

    #[test]
    fn panic_runs_destructors_and_returns_error_sentinel() {
        struct Guard<'a>(&'a AtomicBool);
        impl Drop for Guard<'_> {
            fn drop(&mut self) {
                self.0.store(true, Ordering::SeqCst);
            }
        }
        let dropped = AtomicBool::new(false);
        let result = catch_unwind_or(false, || {
            let _guard = Guard(&dropped);
            panic!("decoder failed");
        });
        assert!(!result);
        assert!(dropped.load(Ordering::SeqCst));
    }

    #[test]
    fn panic_is_caught_before_returning_through_c_abi() {
        extern "C" fn boundary() -> i32 {
            catch_unwind_or(-1, || panic!("decoder failed"))
        }
        assert_eq!(boundary(), -1);
    }

    #[test]
    fn owned_panic_payload_is_caught() {
        let result = catch_unwind_or(-1, || std::panic::panic_any(String::from("decoder failed")));
        assert_eq!(result, -1);
    }
}
