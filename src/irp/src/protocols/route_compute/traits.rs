pub trait RouteComputeEngine {
    type Input;
    type Output;

    fn compute(&self, input: &Self::Input) -> Self::Output;
}
