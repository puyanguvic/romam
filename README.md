# Romam

Welcome to the Romam GitHub repository! Romam is a modular and reconfigurable routing protocol framework designed to facilitate the development, testing, and deployment of advanced network routing protocols. Inspired by the principles of modularity and flexibility, Romam allows researchers and developers to experiment with novel routing strategies and integrate cutting-edge AI-driven decision-making mechanisms.

## Features

- **Modular Design**: Easily interchangeable components for different aspects of routing.
- **Interface-Driven Decoupling**: Separation of routing logic from data collection and forwarding decisions.
- **Support for Advanced Algorithms**: Integration of AI and ML techniques for dynamic routing decisions.
- **Extensive Simulation Support**: Compatible with ns-3 for thorough testing before deployment.

## Installation

### Prerequisites

Ensure you have ns-3.40 installed on your Linux system. If you do not have ns-3.40, follow the installation guide here: [ns-3 Installation Guide](https://www.nsnam.org/docs/release/3.40/tutorial/html/getting-started.html).

### Installing Romam

To integrate Romam with ns-3.40, clone this repository into the `contrib` folder of your ns-3 installation:

```bash
# Navigate to your ns-3.40 `contrib` directory
cd path/to/ns-3.40/contrib

# Clone the Romam repository
git clone https://github.com/yourusername/romam.git

# Navigate to the Romam directory and build the project
cd romam
./ns3 configure
./ns3 build
```

### Examples
Romam comes with a set of routing protocol examples, reconstructed based on the Romam architecture. You can find these examples in the /contrib/romam/examples directory. These examples demonstrate the implementation of various routing protocols using Romam and provide a practical starting point for developing your own protocols.

To run an example, just copy the xxx-example.cc to ns3.40/scratch/, and

```bash
./ns3 run scratch/xxx-example.cc
```

Contributing
We welcome contributions from the community. If you would like to contribute to Romam, please fork the repository, make your changes, and submit a pull request.

License
Romam is licensed under the MIT License. See the LICENSE file for more details.

This README provides a comprehensive overview of Romam, its installation process, and how to run examples, facilitating easy understanding and usability for new users. Adjust the paths and links as necessary to ensure they accurately reflect the structure of your project repository.
