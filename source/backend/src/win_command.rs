//! Spawn helper that suppresses the transient console window child processes
//! would otherwise flash on Windows.
//!
//! The app is a GUI (`windows` subsystem) binary, so it has no console of its
//! own. Every `std::process::Command` we spawn — `git status`, build runners,
//! `nasm`, `radare2`, `explorer`, … — therefore pops a brand-new black `cmd`
//! window for its lifetime. For a one-shot tool that's a flash; for a command
//! the UI polls (e.g. `git status` while a repo is open) it's a relentless
//! flicker that makes the app unusable. `CREATE_NO_WINDOW` runs the child
//! without allocating a console. On non-Windows this is a plain `Command::new`.

use std::ffi::OsStr;
use std::process::Command;

/// Like `Command::new`, but the child never flashes a console window on Windows.
/// Use this for ALL external-process spawns instead of `Command::new`.
pub fn command<S: AsRef<OsStr>>(program: S) -> Command {
    let mut cmd = Command::new(program);
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        const CREATE_NO_WINDOW: u32 = 0x0800_0000;
        cmd.creation_flags(CREATE_NO_WINDOW);
    }
    cmd
}
