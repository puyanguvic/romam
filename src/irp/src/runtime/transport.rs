use std::io;
use std::net::{SocketAddr, UdpSocket};
use std::time::Duration;

pub struct UdpTransport {
    sock: UdpSocket,
    recv_buf_size: usize,
}

impl UdpTransport {
    pub fn bind(bind_address: &str, bind_port: u16, recv_buf_size: usize) -> io::Result<Self> {
        let sock = UdpSocket::bind((bind_address, bind_port))?;
        Ok(Self {
            sock,
            recv_buf_size,
        })
    }

    pub fn recv(&self, timeout: Duration) -> io::Result<Option<(Vec<u8>, SocketAddr)>> {
        self.sock.set_read_timeout(Some(timeout))?;
        let mut buf = vec![0_u8; self.recv_buf_size];
        match self.sock.recv_from(&mut buf) {
            Ok((n, addr)) => {
                buf.truncate(n);
                Ok(Some((buf, addr)))
            }
            Err(err)
                if err.kind() == io::ErrorKind::WouldBlock
                    || err.kind() == io::ErrorKind::TimedOut =>
            {
                Ok(None)
            }
            Err(err) => Err(err),
        }
    }

    pub fn send(&self, payload: &[u8], address: &str, port: u16) -> io::Result<usize> {
        self.sock.send_to(payload, (address, port))
    }
}
