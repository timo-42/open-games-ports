//! Cbuf_Execute applies each command before reading the next one. A map load
//! can run scripts that define or change dvars used by a later exec or vstr.
use super::Server;
use cod2_server::config::command_buffer;
use std::io;

impl Server {
    pub(super) fn execute_console_text(
        &mut self,
        text: &str,
        sources: &mut Vec<String>,
    ) -> io::Result<()> {
        let Some(commands) = command_buffer(text) else {
            self.print_console(b"Cbuf_InsertText overflowed\n");
            return Ok(());
        };
        for words in commands.filter(|words| !words.is_empty()) {
            if let Some(mut insertion) = self.config.command_insertion(&words, &mut &self.assets)? {
                if let Some(message) = insertion.message.take() {
                    self.print_console(format!("{message}\n").as_bytes());
                }
                insertion.expand(sources, |text, sources| {
                    self.execute_console_text(text, sources)
                })?;
            } else {
                let mut output = Vec::new();
                let result = self.operator_command(&words, &mut output);
                self.print_console(&output);
                result?;
            }
            if self.quitting {
                break;
            }
        }
        Ok(())
    }

    pub(super) fn operator(&mut self, words: &[String]) -> Vec<u8> {
        let mut output = Vec::new();
        if let Err(error) = self.operator_command(words, &mut output) {
            output.extend(format!("{error}\n").bytes());
        }
        output
    }

    fn operator_command(&mut self, words: &[String], output: &mut Vec<u8>) -> io::Result<()> {
        if let Some(text) = self.operator_reply(words) {
            output.extend(text);
            return Ok(());
        }
        let mut config = self.config.clone();
        config.execute_tokens_files(words, &self.assets)?;
        for message in config.messages.drain(..) {
            output.extend(message.bytes());
            output.push(b'\n');
        }
        self.quitting = !self.apply_with_output(config, output)?;
        Ok(())
    }
}
