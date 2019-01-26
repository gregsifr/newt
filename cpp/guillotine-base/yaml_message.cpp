#include <guillotine/yaml_message.h>
#include <Util/Socket.h>

#include <cstring>
#include <cstdlib>

#include <boost/lexical_cast.hpp>

using namespace std;

namespace guillotine {
namespace yaml {

    bool base_reader::get_docs ( const char *&c, int &n ) {
        if (buf.empty()) return false;
        lastmark = buf.rfind("\n---");

        if (lastmark != buf.npos) {
            c = buf.c_str();
            n = lastmark+4;
            return true;
        } else { 
            lastmark = 0;
            return false;
        }
    }

    bool base_reader::doc_ready ( ) {
        const char *c;
        int n = 0;
        get_docs(c, n);
        return n > 4;
    }

    void base_reader::clear_docs ( ) {
        buf.erase(0, lastmark);
        lastmark = 0;
    }

    bool base_writer::take_docs ( const char *c, int n ) {
        buf.append(c, n);
        return true;
    }
    void base_writer::clear_docs ( ) {
        buf.erase();
    }

    bool file_writer::write_bytes ( ) {
        out << buf;
        return out.good();
    }

    bool file_writer::ready ( ) {
        return out.good();
    }
    bool file_writer::error ( ) {
        return out.fail();
    }
    bool file_writer::eof ( ) {
        return false;
    }

    bool file_reader::read_bytes ( ) {
        if (in.eof() || in.fail() || in.bad()) {
            return false;
        }
        int rd = 0;
        do {
            in.read(cbuf, 1024);
            rd = in.gcount();
            buf.append(cbuf, rd);
        } while (rd == 1024);
        return true;
    }
    bool file_reader::ready ( ) {
        return in.peek() != EOF ;
    }
    bool file_reader::eof ( ) {
        return in.eof();
    }
    bool file_reader::error ( ) {
        return in.fail() || in.bad();
    }

    bool socket_reader::read_bytes ( ) {

        int i = s->bytesUnread();
        if (i == 0) {
            if (s->readready() == 1) is_eof = true;
            return false;
        }
        
        while (i > 0) {
            int rd = s->read(cbuf, min(i, 1024));
            i -= rd;
            buf.append(cbuf, rd);
        }
        return true;
    }
    bool socket_reader::ready ( ) {
        return s->bytesUnread() > 0;
    }
    bool socket_reader::wait ( ) {
        return s->waitready();
    }
    bool socket_reader::eof ( ) {
        return is_eof || s->readready() == -1;
    }
    bool socket_reader::error ( ) {
        int s_err = 0;
        s->getIntSockOpt(SO_ERROR, &s_err);
        return s_err != 0;
    }

    bool socket_writer::write_bytes ( ) {
        const char *b = buf.c_str();
        int rem = buf.length();
        if (rem == 0) return true;

        int nwrit = 0;
        nwrit = s->write(b, rem);
        if (nwrit > 0) {
            rem -= nwrit;
            buf.erase(0, nwrit);
        } else {
            perror("write");
        }
        return rem == 0;
    }
    bool socket_writer::ready( ) {
        return s->Established();
    }
    bool socket_writer::eof ( ) {
        return false;
    }
    bool socket_writer::error ( ) {
        return false;
    }


    int libyaml::ystrcmp ( const yaml_char_t *a, const yaml_char_t *b ) {
        return strcmp((char *)a, (char *)b);
    }
    yaml_char_t *libyaml::maptag = (yaml_char_t *) YAML_MAP_TAG;
    yaml_char_t *libyaml::seqtag = (yaml_char_t *) YAML_SEQ_TAG;
    yaml_char_t *libyaml::strtag = (yaml_char_t *) YAML_STR_TAG;
    yaml_char_t *libyaml::inttag = (yaml_char_t *) YAML_INT_TAG;
    yaml_char_t *libyaml::flttag = (yaml_char_t *) YAML_FLOAT_TAG;
    yaml_char_t *libyaml::booltag = (yaml_char_t *) YAML_BOOL_TAG;

    void parser::yaml_to_msg ( message &m, yaml_document_t *ydoc, yaml_node_pair_t *start, yaml_node_pair_t *end ) {
        for (yaml_node_pair_t *pair = start; pair != end; pair++) {
            yaml_node_t *key = yaml_document_get_node(ydoc, pair->key);
            yaml_node_t *val = yaml_document_get_node(ydoc, pair->value);
            if (key && val && key->type == YAML_SCALAR_NODE) {
                string key_str((char *)key->data.scalar.value, key->data.scalar.length);
                if (val->type == YAML_SCALAR_NODE) {
                    string val_str((char *)val->data.scalar.value, val->data.scalar.length);
                    if (key_str == "message") {
                        m.name() = val_str;
                    } else {
                        m.insert(make_pair(key_str, node(boost::any(val_str), false)));
                    }
                } else {
                    if (val->type == YAML_SEQUENCE_NODE) {
                        node_list l;
                        for (yaml_node_item_t *item = val->data.sequence.items.start;
                                item != val->data.sequence.items.top;
                                ++item) {
                            yaml_node_t *n = yaml_document_get_node(ydoc, *item);
                            if (n && n->type == YAML_SCALAR_NODE) {
                                string val_str((char *)n->data.scalar.value, n->data.scalar.length);
                                l.push_back(node(boost::any(val_str), false));
                            }
                        }
                        m.insert(make_pair(key_str, node(boost::any(l), false)));
                    }
                }

            }
        }
    }

    bool parser::get_messages ( ) {
        const char *c;
        int n;
        if(!rdr->get_docs(c, n)) return false;

        yaml_parser_t yp;
        yaml_parser_initialize(&yp);

        yaml_parser_set_input_string(&yp, reinterpret_cast<const unsigned char*>(c), n);
        yaml_document_t ydoc;

        while (true) {
            if (!yaml_parser_load(&yp, &ydoc)) {
                break;
            }

            yaml_node_t *root = yaml_document_get_root_node(&ydoc);
            if (!root) {
                yaml_document_delete(&ydoc);
                break;
            }

            if (root->type == YAML_MAPPING_NODE) {
                message m;
                yaml_to_msg(m, &ydoc,
                        root->data.mapping.pairs.start,
                        root->data.mapping.pairs.top);
                ms.push_back(m);
            } else if (root->type == YAML_SEQUENCE_NODE) {
                for (yaml_node_item_t *item = root->data.sequence.items.start;
                        item != root->data.sequence.items.end;
                        ++item) {
                    yaml_node_t *n = yaml_document_get_node(&ydoc, *item);
                    if (n && n->type == YAML_MAPPING_NODE) {
                        message m;
                        yaml_to_msg(m, &ydoc,
                                n->data.mapping.pairs.start,
                                n->data.mapping.pairs.top);
                        ms.push_back(m);
                    }
                }
            }
            yaml_document_delete(&ydoc);
        }

        yaml_parser_delete(&yp);
        rdr->clear_docs();
        return true;
    }
    void parser::init() {
    }


    int emitter::write_handler ( void *thisptr, unsigned char *buf, size_t size ) {
        emitter *me = static_cast<emitter *>(thisptr);
        return me->wtr->take_docs((char *) buf, size);
    }

    void emitter::msg_to_yaml ( yaml_document_t *ydoc, int parent, message &m ) {

        int dict = yaml_document_add_mapping(ydoc, libyaml::maptag, YAML_BLOCK_MAPPING_STYLE);
        
        if (!m.name().empty()) {
            int key = yaml_document_add_scalar(ydoc, libyaml::strtag, (yaml_char_t *) "message", 7, 
                    YAML_PLAIN_SCALAR_STYLE);
            int val = yaml_document_add_scalar(ydoc, libyaml::strtag, 
                    (yaml_char_t *) m.name().c_str(), m.name().length(), YAML_PLAIN_SCALAR_STYLE);
            yaml_document_append_mapping_pair(ydoc, dict, key, val);
        }

        for (message::const_iterator fi = m.begin(); fi != m.end(); ++fi) {
            const string &key_str = fi->first;
            int key = yaml_document_add_scalar(ydoc, libyaml::strtag,
                    (yaml_char_t *) key_str.c_str(), key_str.length(), YAML_PLAIN_SCALAR_STYLE);
            int val = 0;

            const string *val_p;
            const node_list *l_p;
            if ((val_p = boost::any_cast<string>(&(fi->second.value()))) != 0) {
                val = yaml_document_add_scalar(ydoc, libyaml::strtag,
                        (yaml_char_t *) val_p->c_str(), val_p->length(), YAML_PLAIN_SCALAR_STYLE);
            } else if ((l_p = boost::any_cast<node_list>(&(fi->second.value()))) != 0) {
                val = yaml_document_add_sequence(ydoc, libyaml::seqtag, YAML_FLOW_SEQUENCE_STYLE);
                for (node_list::const_iterator li = l_p->begin(); li != l_p->end(); ++li) {
                    if ((val_p = boost::any_cast<string>(&(li->value()))) != 0) {
                        int lval = yaml_document_add_scalar(ydoc, libyaml::strtag,
                                (yaml_char_t *) val_p->c_str(), val_p->length(), YAML_PLAIN_SCALAR_STYLE);
                        yaml_document_append_sequence_item(ydoc, val, lval);
                    }
                }
            }
            if (val != 0)
                yaml_document_append_mapping_pair(ydoc, dict, key, val);
        }

        yaml_document_append_sequence_item(ydoc, parent, dict);
    }

    bool emitter::write_messages ( ) {
        
        yaml_emitter_t ye;
        yaml_document_t ydoc;
        yaml_emitter_initialize(&ye);
        yaml_document_initialize(&ydoc, 0, 0, 0, 0, 0);

        yaml_emitter_set_output(&ye, write_handler, (void *) this);
        yaml_emitter_set_encoding(&ye, YAML_UTF8_ENCODING);
        yaml_emitter_set_canonical(&ye, 0);
        yaml_emitter_set_indent(&ye, 4);
        yaml_emitter_set_width(&ye, 80);
        yaml_emitter_set_unicode(&ye, 0); // ASCII only
        yaml_emitter_set_break(&ye, YAML_LN_BREAK);
      
        int pseq = yaml_document_add_sequence(&ydoc, libyaml::seqtag, YAML_BLOCK_SEQUENCE_STYLE);

        for (list<message>::iterator mi = ms.begin(); mi != ms.end(); ++mi) {
            msg_to_yaml(&ydoc, pseq, *mi);
        }
        ydoc.end_implicit = 1;

        yaml_emitter_open(&ye);
        yaml_emitter_dump(&ye, &ydoc);


        yaml_emitter_flush(&ye);
        yaml_emitter_close(&ye);
        yaml_emitter_delete(&ye);
        yaml_document_delete(&ydoc);

        // Ugh ugh ugh ugh
        wtr->take_docs("---\n", 4);


        return true;
    };

}
}
