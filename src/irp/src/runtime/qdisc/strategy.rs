use std::collections::BTreeMap;
use std::str::FromStr;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum QdiscKind {
    Fifo,
    PfifoFast,
    Ecn,
    Red,
    FqCodel,
    Prio,
    Drr,
    Netem,
    Tbf,
}

impl QdiscKind {
    pub fn as_tc_name(self) -> &'static str {
        match self {
            QdiscKind::Fifo => "pfifo",
            QdiscKind::PfifoFast => "pfifo_fast",
            // Linux ECN queueing is typically expressed via RED + ecn flag.
            QdiscKind::Ecn => "red",
            QdiscKind::Red => "red",
            QdiscKind::FqCodel => "fq_codel",
            QdiscKind::Prio => "prio",
            QdiscKind::Drr => "drr",
            QdiscKind::Netem => "netem",
            QdiscKind::Tbf => "tbf",
        }
    }
}

impl FromStr for QdiscKind {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.trim().to_ascii_lowercase().as_str() {
            "fifo" | "pfifo" => Ok(Self::Fifo),
            "pfifo_fast" | "pfifofast" => Ok(Self::PfifoFast),
            "ecn" => Ok(Self::Ecn),
            "red" => Ok(Self::Red),
            "fq_codel" | "fqcodel" => Ok(Self::FqCodel),
            "prio" => Ok(Self::Prio),
            "drr" => Ok(Self::Drr),
            "netem" => Ok(Self::Netem),
            "tbf" => Ok(Self::Tbf),
            other => Err(format!("unsupported qdisc kind: {other}")),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct QdiscProfile {
    pub kind: QdiscKind,
    pub handle: Option<String>,
    pub parent: Option<String>,
    pub params: BTreeMap<String, String>,
}

impl QdiscProfile {
    pub fn new(kind: QdiscKind) -> Self {
        Self {
            kind,
            handle: None,
            parent: None,
            params: BTreeMap::new(),
        }
    }
}
