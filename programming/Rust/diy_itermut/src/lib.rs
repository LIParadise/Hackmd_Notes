//! https://quinedot.github.io/rust-learning/lt-ex-mut-slice.html

pub struct MyIterMut<'a, T> {
    slice: &'a mut [T],
}

impl<'a, T> Iterator for MyIterMut<'a, T> {
    type Item = &'a mut T;
    fn next(&mut self) -> Option<Self::Item> {
        let slice = std::mem::replace(&mut self.slice, <&mut [T]>::default());
        let (first, mut tail) = slice.split_first_mut()?;
        std::mem::swap(&mut self.slice, &mut tail);
        Some(first)
    }
}

pub struct QuineDotIterMut<'a, T> {
    slice: &'a mut [T],
}

impl<'a, T> Iterator for QuineDotIterMut<'a, T> {
    type Item = &'a mut T;
    fn next(&mut self) -> Option<Self::Item> {
        match std::mem::take(&mut self.slice) {
            [] => None,
            [first, tail @ ..] => {
                self.slice = tail;
                Some(first)
            }
        }
    }
}

impl<'a, T> MyIterMut<'a, T> {
    pub fn from<Arr: std::ops::DerefMut<Target = [T]>>(arr: &'a mut Arr) -> Self {
        let slice = &mut *arr;
        Self { slice }
    }
}

impl<'a, T> QuineDotIterMut<'a, T> {
    pub fn from<Arr: std::ops::DerefMut<Target = [T]>>(arr: &'a mut Arr) -> Self {
        let slice = &mut *arr;
        Self { slice }
    }
}
