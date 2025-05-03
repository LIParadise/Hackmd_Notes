use diy_itermut::MyIterMut;

fn main() {
    let mut strings = ["yjsp", "114514", "sakanaction"]
        .into_iter()
        .map(String::from)
        .collect::<Vec<_>>();

    dbg!(&strings);
    for s in MyIterMut::from(&mut strings) {
        s.push('!');
    }
    dbg!(&strings);
}
