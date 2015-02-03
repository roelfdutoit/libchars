/*
Copyright (C) 2013-2014 Roelof Nico du Toit.

@description Editor implementation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. 
*/

#include "editor.h"
#include "debug.h"

namespace libchars {

    struct decode_sequence_t { const char *sequence; key_e result; };

    decode_sequence_t __decode_table[] =
    {
        { "\x1b[D", KEY_LEFT },
        { "\x1b[C", KEY_RIGHT },
        { "\x1b[A", KEY_UP },
        { "\x1b[B", KEY_DOWN },
        { "\x1b[5~", KEY_PGUP },
        { "\x1b[6~", KEY_PGDN },
        { "\x1b[3~", KEY_DEL },
        { "\x7f", KEY_BKSP },
        { "\x01", KEY_SOL },
        { "\x03", KEY_QUIT },
        { "\x04", KEY_DEL }, // ^D (will be translated into KEY_EOF in multiline mode)
        { "\x05", KEY_EOL },
        { "\x08", KEY_BKSP },
        { "\x09", KEY_TAB },
        { "\x0b", KEY_WIPE },
        { "\x0c", KEY_CLEAR },
        { "\x0d", KEY_ENTER },
        { "\x14", KEY_SWAP },
        { "\x1a", KEY_EOF }, // ^Z (only used in multiline mode)

        { "\x1b\x1b", IGNORE_SEQ },
        { "\x1b[H", IGNORE_SEQ },
        { "\x1b[F", IGNORE_SEQ },
        { "\x1bO", IGNORE_SEQ },
        { "\x1b[1", IGNORE_SEQ },
        { "\x1b[2", IGNORE_SEQ },
    };

    const static size_t DECODE_TABLE_ENTRIES = (sizeof(__decode_table) / sizeof(decode_sequence_t));

    key_e editor::decode_key(uint8_t c)
    {
        if (seq_N < MAX_DECODE_SEQUENCE) {
            seq[seq_N++] = c;
            size_t idx;
            for (idx = 0; idx < DECODE_TABLE_ENTRIES; ++idx) {
                const char *T_seq = __decode_table[idx].sequence;
                size_t T_seq_N = strlen(T_seq);
                if (memcmp(T_seq, seq, seq_N) == 0) {
                    if (T_seq_N == seq_N) {
                        seq_N = 0;
                        if (obj->mode == MODE_MULTILINE && __decode_table[idx].result == KEY_DEL)
                            return KEY_EOF;
                        else
                            return __decode_table[idx].result;
                    }
                    else if (T_seq_N > seq_N) {
                        return PARTIAL_SEQ;
                    }
                }
            }
        }

        if (seq_N>1 || !isprint(c)) {
            size_t i;
            char buffer[256] = "";
            for (i=0; i<seq_N; ++i) {
                uint8_t cc = seq[i];
                if (isprint(cc))
                    sprintf(buffer+strlen(buffer), "%c", cc);
                else
                    sprintf(buffer+strlen(buffer), "\\x%02x", cc);
            }
            LC_LOG_DEBUG("UNKNOWN SEQUENCE: --> %s <--",buffer);
        }

        seq_N = 0;

        if (obj->mode == MODE_COMMAND && c == '?')
            return KEY_HELP;
        else if (isprint(c))
            return PRINTABLE_CHAR;
        else
            return IGNORE_SEQ;
    }

    void editor::request_render()
    {
        if (state == IDLE || state == RENDER_DEFER) {
            if (driver.read_available())
                state = RENDER_DEFER;
            else
                state = RENDER_NOW;
        }
    }

    size_t editor::render(size_t buf_idx, size_t length)
    {
        std::string sequence;
        size_t n = obj->render(obj->idx(buf_idx),length,sequence);
        if (n > 0)
            driver.write(sequence.data(),sequence.length());
        return n;
    }

    int editor::print()
    {
        state = IDLE;

        if (obj->mode == MODE_MULTILINE) {
            // render prompt (if not already rendered)
            if (obj->prompt_rendered == 0) {
                driver.write(obj->prompt.data(), obj->prompt.length());
                obj->prompt_rendered = obj->prompt.length();
            }
            // print from previous cursor up to current end-of-buffer
            if (obj->cursor < obj->insert_idx) {
                size_t n_to_write = (obj->insert_idx - obj->cursor);
                driver.write(obj->value().data() + obj->cursor, n_to_write);
                obj->cursor = obj->insert_idx;
            }
        }
        else if (!driver.control()) {
            // render prompt (if not already rendered)
            if (obj->prompt_rendered == 0) {
                driver.write(obj->prompt.data(), obj->prompt.length());
                obj->prompt_rendered = obj->prompt.length();
            }
            // render from previous position up to current end-of-buffer
            if (obj->cursor < obj->insert_idx) {
                size_t start = obj->terminal_idx(obj->idx(obj->cursor));
                size_t end = obj->terminal_idx(obj->idx(obj->insert_idx + 1));
                size_t render_length = (obj->mode == MODE_PASSWORD || end <= start) ? 0 : (end - start);
                obj->cursor += render(obj->cursor,render_length);
            }
        } 
        else {
          size_t cols = driver.columns();
          size_t rows = driver.rows();

          // invalid: no rows or columns
          if (cols == 0 || rows == 0)
              return -2;
          // do nothing if only 1 position available (1 x 1 terminal)
          if (cols <= 1 && rows <= 1)
              return -3;

          // calculate available terminal space (leave extra space at the end for the cursor to overflow into)
          size_t window = cols * rows - 1;

          // dump will be limited depending on available terminal space;
          // prompt will be shortened if terminal not big enough for full string

          size_t cursor = obj->terminal_cursor(obj->idx(obj->insert_idx));
          size_t start = obj->terminal_idx(obj->idx(obj->insert_idx));
          size_t end = obj->terminal_idx(obj->idx(obj->insert_idx + 1));

          size_t render_length = (obj->mode == MODE_PASSWORD) ? 0 : obj->terminal_idx(obj->idx(obj->length()));

          if (cursor < start)
              start = cursor;
          if (end < start)
              end = start;
          if ((cursor - start) > window)
              cursor = start;

          LC_LOG_VERBOSE("window[%zu:%zux%zu];start[%zu];cursor[%zu];end[%zu];render_len[%zu]",window,cols,rows,start,cursor,end,render_length);

          if ((render_length + obj->prompt.length()) > window) {
              size_t idx_from = 0;
              size_t from = 0;
              if (render_length > 0) {
                  if ((end - start) >= window) {
                      idx_from = obj->insert_idx;
                      from = start;
                  }
                  else {
                      size_t search_from = 0;
                      if ((render_length - cursor) <= window/2) { // <-- cursor in last 1/2 window
                          // find all complete expanded characters that fit into (window - (LENGTH - START)) before START
                          if (render_length > window)
                              search_from = render_length - window;
                          else
                              search_from = 0;
                      }
                      else if (cursor < window/2) { // <-- cursor in first 1/2 window
                          // find all complete expanded characters that fit into space before START
                          search_from = 0;
                      }
                      else {
                          search_from = cursor - window/2;
                      }
                      LC_LOG_VERBOSE("search_from[%zu];start[%zu]",search_from,start);

                      if (search_from < start) {
                          // start searching with assumption that render is 1:1 with buffer
                          size_t space = start - search_from;
                          if (space < obj->insert_idx)
                              idx_from = obj->insert_idx - space;
                          else
                              idx_from = 0;
                          // keep on comparing until rendered string fits in or until space runs out
                          from = obj->terminal_idx(obj->idx(idx_from));
                          if (from > search_from) {
                              // more space available; search backwards
                              while (idx_from > 0 && from > search_from) {
                                  --idx_from;
                                  from = obj->terminal_idx(obj->idx(idx_from));
                              }
                              if (from < search_from) {
                                  ++idx_from;
                                  from = obj->terminal_idx(obj->idx(idx_from));
                              }
                          }
                          else if (from < search_from) {
                              // used too much space; search forwards
                              while (idx_from < obj->insert_idx && from < search_from) {
                                  ++idx_from;
                                  from = obj->terminal_idx(obj->idx(idx_from));
                              }
                          }
                      }
                      else {
                          idx_from = obj->insert_idx;
                          from = start;
                      }
                  }

                  LC_LOG_VERBOSE("from[%zu];idx_from[%zu]",from,idx_from);

                  // find all expanded characters (even partial) that fit into (window - END) on and after END
                  size_t search_to = from + window;
                  if (search_to > render_length)
                      search_to = render_length;
                  size_t idx_to = obj->insert_idx;
                  if (idx_to < idx_from)
                      idx_to = idx_from;
                  size_t to = obj->terminal_idx(obj->idx(idx_to + 1));

                  LC_LOG_VERBOSE("search_to[%zu];end[%zu]",search_to,end);

                  if (search_to > end) {
                      // start searching with assumption that render is 1:1 with buffer
                      idx_to = obj->idx(obj->insert_idx + (search_to - end));
                      // keep on comparing until rendered string fits in or until space runs out
                      to = obj->terminal_idx(obj->idx(idx_to + 1));
                      if (to < search_to) {
                          // more space available; search forwards
                          while (idx_to < obj->length() && to < search_to) {
                              ++idx_to;
                              to = obj->terminal_idx(obj->idx(idx_to + 1));
                          }
                          if (to > search_to && idx_to > obj->insert_idx) {
                              --idx_to;
                              to = obj->terminal_idx(obj->idx(idx_to + 1));
                          }
                      }
                      else if (to > search_to) {
                          // used too much space; search backwards
                          while (idx_to > obj->insert_idx && to > search_to) {
                              --idx_to;
                              to = obj->terminal_idx(obj->idx(idx_to + 1));
                          }
                      }
                  }

                  LC_LOG_VERBOSE("to[%zu];idx_to[%zu]",to,idx_to);

                  if (to < from)
                      to = from;
                  else if ((to - from) > window)
                      to = from + window;

                  render_length = to - from;
              }

              // start printing in upper-left corner
              terminal_driver::auto_cursor __(driver);
              driver.clear_screen();

              // print partial prompt (if possible)
              if (render_length < window) {
                  obj->prompt_rendered = window - render_length;
                  driver.write(obj->prompt.data() + obj->prompt.length() - obj->prompt_rendered, obj->prompt_rendered);
                  LC_LOG_VERBOSE("prompt_rendered[%zu]:%s",obj->prompt_rendered,obj->prompt.c_str()+obj->prompt.length()-obj->prompt_rendered);
              }
              else {
                  obj->prompt_rendered = 0;
                  render_length = window;
              }

              // print line[idx_from..]
              size_t rendered = render(idx_from, render_length);
              // move to final cursor position (relative to current position)
              LC_LOG_VERBOSE("cursor[%zu];from[%zu];rendered[%zu]",cursor,from,rendered);
              if (driver.set_new_xy((ssize_t)cursor - (ssize_t)(from + rendered)) < 0)
                  return -1;
          }
          else {
              LC_LOG_VERBOSE("obj->cursor[%zu]",obj->cursor);

              // move cursor to start of prompt position (relative to current position)
              if (driver.set_new_xy(0 - (ssize_t)obj->cursor - (ssize_t)obj->prompt_rendered) < 0)
                  return -1;

              // clear area for printing (TODO: optimize by only clearing specific lines)
              driver.clear_to_end_of_screen();

              // render prompt
              if (obj->prompt.length() > 0) {
                  driver.write(obj->prompt.data(), obj->prompt.length());
                  LC_LOG_VERBOSE("cursor[%zu];prompt_rendered[%zu]",obj->cursor,obj->prompt_rendered);
                  obj->cursor = 0;
              }
              obj->prompt_rendered = obj->prompt.length();

              if (render_length > 0) {
                  // print line
                  terminal_driver::auto_cursor __(driver);
                  size_t rendered = render(0, render_length);
                  LC_LOG_VERBOSE("rendered[%zu]",rendered);

                  // hack to convince cursor to move to the start of the next line
                  // on a fully populated rendered line
                  if (((obj->prompt_rendered + rendered) % cols) == 0)
                      driver.newline();

                  // move to final cursor position (relative to current position)
                  if (driver.set_new_xy((ssize_t)cursor - (ssize_t)rendered) < 0)
                      return -1;
              }
          }

          obj->cursor = (render_length > 0) ? cursor : 0;

          if (LC_LOG_CHECK_LEVEL(debug::VERBOSE) && driver.control()) {
              size_t x,y;
              driver.cursor_position(x,y);
              LC_LOG_VERBOSE("obj->cursor[%zu];x[%zu],y[%zu]",obj->cursor,x,y);
          }
        }

        return 0;
    }

    int editor::edit(edit_object &obj_ref)
    {
        //TODO: remove newlines from prompt (not for multi-line mode)

        //TODO: prevent re-entrant calls

        obj = &obj_ref;

        //XXX assume cursor position has not shifted since last edit
        print();

        uint8_t c;
        int r;
        while ((r = driver.read(c)) >= 0) {
            if (driver.size_changed() && obj->mode == MODE_COMMAND) {
                // clear screen because position is not reliable after terminal size update
                driver.clear_screen();
                obj->prompt_rendered = 0;
                print();
            }
            if (r > 0) {
                k = decode_key(c);
                switch (k) {
                case PRINTABLE_CHAR:
                    obj->insert(c);
                    request_render();
                    break;
                case KEY_LEFT:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->left();
                        request_render();
                    }
                    break;
                case KEY_RIGHT:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->right();
                        request_render();
                    }
                    break;
                case KEY_WIPE:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->wipe();
                        request_render();
                    }
                    break;
                case KEY_CLEAR:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->rewind();
                        driver.clear_screen();
                        print();
                    }
                    break;
                case KEY_SWAP:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->swap();
                        request_render();
                    }
                    break;
                case KEY_DEL:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->del();
                        request_render();
                    }
                    break;
                case KEY_BKSP:
                    if (obj->mode == MODE_MULTILINE) {
                        //TODO: multiline backspace
                    }
                    else if (driver.control()) {
                        obj->bksp();
                        request_render();
                    }
                    break;
                case KEY_SOL:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->left(obj->length());
                        request_render();
                    }
                    break;
                case KEY_EOL:
                    if (obj->mode == MODE_COMMAND && driver.control()) {
                        obj->right(obj->length());
                        request_render();
                    }
                    break;
                case KEY_ENTER:
                    switch (obj->mode) {
                    case MODE_STRING:
                    case MODE_PASSWORD:
                        print();
                        return 0;
                    case MODE_MULTILINE:
                        obj->insert('\n');
                        request_render();
                        break;
                    case MODE_COMMAND:
                        if (obj->key_valid(KEY_ENTER)) {
                            print();
                            return 0;
                        }
                        break;
                    }
                    break;
                case KEY_EOF:
                    if (obj->mode == MODE_MULTILINE)
                        return 0;
                    break;
                case KEY_QUIT:
                    print();
                    return 0;
                case KEY_TAB:
                case KEY_HELP:
                case KEY_UP:
                case KEY_DOWN:
                case KEY_PGUP:
                case KEY_PGDN:
                    if (obj->mode == MODE_COMMAND && obj->key_valid(k)) {
                        print();
                        return 0;
                    }
                    break;
                case PARTIAL_SEQ:
                case IGNORE_SEQ:
                    break;// ignore sequence
                }

                if (must_render())
                    print();
            }
        }
        k = IGNORE_SEQ;
        return -1;
    }

    int editor::edit(std::string &str)
    {
        edit_object O_tmp(MODE_STRING,str);
        int r = edit(O_tmp);
        if (r == 0) str = O_tmp.value();
        return r;
    }

}

