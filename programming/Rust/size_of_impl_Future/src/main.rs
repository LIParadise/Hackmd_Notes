use std::pin::{Pin, pin};
use std::time::Duration;
use tokio::time::{sleep, timeout};
use tokio::{select, spawn};

fn main() {
    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_time()
        .build()
        .unwrap();

    let fut = async {
        const SHORT: u64 = 1375;
        const LONG: u64 = 1879;
        let mut sleep_short = pin!(sleep(Duration::from_millis(SHORT)));
        let mut sleep_long = pin!(sleep(Duration::from_millis(LONG)));
        loop {
            select! {
                _ = &mut sleep_short => {
                    println!("short!");
                    sleep_short.set(sleep(Duration::from_millis(SHORT)));
                }
                _ = &mut sleep_long => {
                    println!("long!");
                    sleep_long.set(sleep(Duration::from_millis(LONG)));
                }
            }
        }
    };
    println!("{}", std::mem::size_of_val(&fut));
    let fut = async move { spawn(fut).await };
    println!("{}", std::mem::size_of_val(&fut));

    {
        let fut = &mut async {
            const SHORT: u64 = 1375;
            const LONG: u64 = 1879;
            spawn(async {
                let mut sleep_short = pin!(sleep(Duration::from_millis(SHORT)));
                let mut sleep_long = pin!(sleep(Duration::from_millis(LONG)));
                loop {
                    select! {
                        _ = &mut sleep_short => {
                            println!("short!");
                            sleep_short.set(sleep(Duration::from_millis(SHORT)));
                        }
                        _ = &mut sleep_long => {
                            println!("long!");
                            sleep_long.set(sleep(Duration::from_millis(LONG)));
                        }
                    }
                }
            })
            .await
            .unwrap()
        };

        {
            let x = &mut Vec::<()>::new();
            assert_eq!(
                size_of::<Vec<String>>(),
                std::mem::size_of_val(x),
                "demonstrating that `std::mem::size_of_val` and `&mut T` works",
            );
        }

        println!("{}", std::mem::size_of_val(fut));
        rt.block_on(async {
            _ = timeout(Duration::from_secs(5), unsafe {
                // Safety:
                // 1. Local scope storage is not destroyed before `block_on` returns
                // 2. No one but us uses this `&mut impl Future`
                Pin::new_unchecked(fut)
            })
            .await;
        });
    }

    println!("exited block");
    rt.block_on(async {
        _ = spawn(async { _ = timeout(Duration::from_secs(5), fut).await }).await;
    });
    println!("FIN");
}
