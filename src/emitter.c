
#include "yaml_private.h"

/*
 * Flush the buffer if needed.
 */

#define FLUSH(emitter)                                                          \
    ((emitter->output.pointer+5 < emitter->output.capacity)                     \
     || yaml_emitter_flush(emitter))

/*
 * Put a character to the output buffer.
 */

#define PUT(emitter,value)                                                      \
    (FLUSH(emitter)                                                             \
     && (JOIN_OCTET(emitter->output,(yaml_char_t)(value)),                      \
         emitter->column ++,                                                    \
         1))

/*
 * Put a line break to the output buffer.
 */

#define PUT_BREAK(emitter)                                                      \
    (FLUSH(emitter)                                                             \
     && ((emitter->line_break == YAML_CR_BREAK ?                                \
             JOIN_OCTET(emitter->output, (yaml_char_t) '\r') :                  \
          emitter->line_break == YAML_LN_BREAK ?                                \
             JOIN_OCTET(emitter->output, (yaml_char_t) '\n') :                  \
          emitter->line_break == YAML_CRLN_BREAK ?                              \
             (JOIN_OCTET(emitter->output, (yaml_char_t) '\r'),                  \
              JOIN_OCTET(emitter->output, (yaml_char_t) '\n')) : 0),            \
         emitter->column = 0,                                                   \
         emitter->line ++,                                                      \
         1))

/*
 * Copy a character from a string into buffer.
 */

#define WRITE(emitter,string)                                                   \
    (FLUSH(emitter)                                                             \
     && (COPY(emitter->output,string),                                          \
         emitter->column ++,                                                    \
         1))

/*
 * Copy a line break character from a string into buffer.
 */

#define WRITE_BREAK(emitter,string)                                             \
    (FLUSH(emitter)                                                             \
     && (CHECK(string,'\n') ?                                                   \
         (PUT_BREAK(emitter),                                                   \
          string.pointer ++,                                                    \
          1) :                                                                  \
         (COPY(emitter->output,string),                                         \
          emitter->column = 0,                                                  \
          emitter->line ++,                                                     \
          1)))

/*
 * API functions.
 */

YAML_DECLARE(int)
yaml_emitter_emit(yaml_emitter_t *emitter, yaml_event_t *event);

/*
 * Utility functions.
 */

static int
yaml_emitter_need_more_events(yaml_emitter_t *emitter);

static int
yaml_emitter_append_tag_directive(yaml_emitter_t *emitter,
        yaml_tag_directive_t value, int allow_duplicates);

static int
yaml_emitter_increase_indent(yaml_emitter_t *emitter,
        int flow, int indentless);

/*
 * State functions.
 */

static int
yaml_emitter_state_machine(yaml_emitter_t *emitter, yaml_event_t *event);

static int
yaml_emitter_emit_stream_start(yaml_emitter_t *emitter,
        yaml_event_t *event);

static int
yaml_emitter_emit_document_start(yaml_emitter_t *emitter,
        yaml_event_t *event, int first);

static int
yaml_emitter_emit_document_content(yaml_emitter_t *emitter,
        yaml_event_t *event);

static int
yaml_emitter_emit_document_end(yaml_emitter_t *emitter,
        yaml_event_t *event);

static int
yaml_emitter_emit_flow_sequence_item(yaml_emitter_t *emitter,
        yaml_event_t *event, int first);

static int
yaml_emitter_emit_flow_mapping_key(yaml_emitter_t *emitter,
        yaml_event_t *event, int first);

static int
yaml_emitter_emit_flow_mapping_value(yaml_emitter_t *emitter,
        yaml_event_t *event, int simple);

static int
yaml_emitter_emit_block_sequence_item(yaml_emitter_t *emitter,
        yaml_event_t *event, int first);

static int
yaml_emitter_emit_block_mapping_key(yaml_emitter_t *emitter,
        yaml_event_t *event, int first);

static int
yaml_emitter_emit_block_mapping_value(yaml_emitter_t *emitter,
        yaml_event_t *event, int simple);

static int
yaml_emitter_emit_node(yaml_emitter_t *emitter, yaml_event_t *event,
        int is_root, int is_sequence, int is_mapping, int is_simple_key);

static int
yaml_emitter_emit_alias(yaml_emitter_t *emitter, yaml_event_t *event);

static int
yaml_emitter_emit_scalar(yaml_emitter_t *emitter, yaml_event_t *event);

static int
yaml_emitter_emit_sequence_start(yaml_emitter_t *emitter, yaml_event_t *event);

static int
yaml_emitter_emit_mapping_start(yaml_emitter_t *emitter, yaml_event_t *event);

/*
 * Checkers.
 */

static int
yaml_emitter_check_empty_document(yaml_emitter_t *emitter);

static int
yaml_emitter_check_empty_sequence(yaml_emitter_t *emitter);

static int
yaml_emitter_check_empty_mapping(yaml_emitter_t *emitter);

static int
yaml_emitter_check_simple_key(yaml_emitter_t *emitter);

static int
yaml_emitter_select_scalar_style(yaml_emitter_t *emitter, yaml_event_t *event);

/*
 * Processors.
 */

static int
yaml_emitter_process_anchor(yaml_emitter_t *emitter);

static int
yaml_emitter_process_tag(yaml_emitter_t *emitter);

static int
yaml_emitter_process_scalar(yaml_emitter_t *emitter);

/*
 * Analyzers.
 */

static int
yaml_emitter_analyze_version_directive(yaml_emitter_t *emitter,
        yaml_version_directive_t version_directive);

static int
yaml_emitter_analyze_tag_directive(yaml_emitter_t *emitter,
        yaml_tag_directive_t tag_directive);

static int
yaml_emitter_analyze_anchor(yaml_emitter_t *emitter,
        yaml_char_t *anchor, int alias);

static int
yaml_emitter_analyze_tag(yaml_emitter_t *emitter,
        yaml_char_t *tag);

static int
yaml_emitter_analyze_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length);

static int
yaml_emitter_analyze_event(yaml_emitter_t *emitter,
        yaml_event_t *event);

/*
 * Writers.
 */

static int
yaml_emitter_write_bom(yaml_emitter_t *emitter);

static int
yaml_emitter_write_indent(yaml_emitter_t *emitter);

static int
yaml_emitter_write_indicator(yaml_emitter_t *emitter,
        char *indicator, int need_whitespace,
        int is_whitespace, int is_indention);

static int
yaml_emitter_write_anchor(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length);

static int
yaml_emitter_write_tag_handle(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length);

static int
yaml_emitter_write_tag_content(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int need_whitespace);

static int
yaml_emitter_write_plain_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int allow_breaks);

static int
yaml_emitter_write_single_quoted_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int allow_breaks);

static int
yaml_emitter_write_double_quoted_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int allow_breaks);

static int
yaml_emitter_determine_chomping(yaml_emitter_t *emitter,
        yaml_string_t string);

static int
yaml_emitter_write_literal_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length);

static int
yaml_emitter_write_folded_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length);

/*
 * Emit an event.
 */

YAML_DECLARE(int)
yaml_emitter_emit(yaml_emitter_t *emitter, yaml_event_t *event)
{
    if (!ENQUEUE(emitter, emitter->events, *event)) {
        yaml_event_delete(event);
        return 0;
    }

    while (!yaml_emitter_need_more_events(emitter)) {
        if (!yaml_emitter_analyze_event(emitter,
                    emitter->events.list + emitter->events.head))
            return 0;
        if (!yaml_emitter_state_machine(emitter,
                    emitter->events.list + emitter->events.head))
            return 0;
        yaml_event_delete(&DEQUEUE(emitter, emitter->events));
    }

    return 1;
}

/*
 * Check if we need to accumulate more events before emitting.
 *
 * We accumulate extra
 *  - 1 event for DOCUMENT-START
 *  - 2 events for SEQUENCE-START
 *  - 3 events for MAPPING-START
 */

static int
yaml_emitter_need_more_events(yaml_emitter_t *emitter)
{
    int level = 0;
    int accumulate = 0;
    size_t idx;

    if (QUEUE_EMPTY(emitter, emitter->events))
        return 1;

    switch (emitter->events.list[emitter->events.head].type) {
        case YAML_DOCUMENT_START_EVENT:
            accumulate = 1;
            break;
        case YAML_SEQUENCE_START_EVENT:
            accumulate = 2;
            break;
        case YAML_MAPPING_START_EVENT:
            accumulate = 3;
            break;
        default:
            return 0;
    }

    if (emitter->events.tail - emitter->events.head > accumulate)
        return 0;

    for (idx = emitter->events.head; idx < emitter->events.tail; idx ++) {
        yaml_event_t *event = emitter->events.list+idx;
        switch (event->type) {
            case YAML_STREAM_START_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_SEQUENCE_START_EVENT:
            case YAML_MAPPING_START_EVENT:
                level += 1;
                break;
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_SEQUENCE_END_EVENT:
            case YAML_MAPPING_END_EVENT:
                level -= 1;
                break;
            default:
                break;
        }
        if (!level)
            return 0;
    }

    return 1;
}

/*
 * Append a directive to the directives stack.
 */

static int
yaml_emitter_append_tag_directive(yaml_emitter_t *emitter,
        yaml_tag_directive_t value, int allow_duplicates)
{
    int idx;
    yaml_tag_directive_t copy = { NULL, NULL };

    for (idx = 0; idx < emitter->tag_directives.length; idx ++) {
        yaml_tag_directive_t *tag_directive = emitter->tag_directives.list+idx;
        if (strcmp((char *)value.handle, (char *)tag_directive->handle) == 0) {
            if (allow_duplicates)
                return 1;
            return EMITTER_ERROR_INIT(emitter, "duplicate %TAG directive");
        }
    }

    copy.handle = yaml_strdup(value.handle);
    copy.prefix = yaml_strdup(value.prefix);
    if (!copy.handle || !copy.prefix) {
        MEMORY_ERROR_INIT(emitter);
        goto error;
    }

    if (!PUSH(emitter, emitter->tag_directives, copy))
        goto error;

    return 1;

error:
    yaml_free(copy.handle);
    yaml_free(copy.prefix);
    return 0;
}

/*
 * Increase the indentation level.
 */

static int
yaml_emitter_increase_indent(yaml_emitter_t *emitter,
        int flow, int indentless)
{
    if (!PUSH(emitter, emitter->indents, emitter->indent))
        return 0;

    if (emitter->indent < 0) {
        emitter->indent = flow ? emitter->best_indent : 0;
    }
    else if (!indentless) {
        emitter->indent += emitter->best_indent;
    }

    return 1;
}

/*
 * State dispatcher.
 */

static int
yaml_emitter_state_machine(yaml_emitter_t *emitter, yaml_event_t *event)
{
    switch (emitter->state)
    {
        case YAML_EMIT_STREAM_START_STATE:
            return yaml_emitter_emit_stream_start(emitter, event);

        case YAML_EMIT_FIRST_DOCUMENT_START_STATE:
            return yaml_emitter_emit_document_start(emitter, event, 1);

        case YAML_EMIT_DOCUMENT_START_STATE:
            return yaml_emitter_emit_document_start(emitter, event, 0);

        case YAML_EMIT_DOCUMENT_CONTENT_STATE:
            return yaml_emitter_emit_document_content(emitter, event);

        case YAML_EMIT_DOCUMENT_END_STATE:
            return yaml_emitter_emit_document_end(emitter, event);

        case YAML_EMIT_FLOW_SEQUENCE_FIRST_ITEM_STATE:
            return yaml_emitter_emit_flow_sequence_item(emitter, event, 1);

        case YAML_EMIT_FLOW_SEQUENCE_ITEM_STATE:
            return yaml_emitter_emit_flow_sequence_item(emitter, event, 0);

        case YAML_EMIT_FLOW_MAPPING_FIRST_KEY_STATE:
            return yaml_emitter_emit_flow_mapping_key(emitter, event, 1);

        case YAML_EMIT_FLOW_MAPPING_KEY_STATE:
            return yaml_emitter_emit_flow_mapping_key(emitter, event, 0);

        case YAML_EMIT_FLOW_MAPPING_SIMPLE_VALUE_STATE:
            return yaml_emitter_emit_flow_mapping_value(emitter, event, 1);

        case YAML_EMIT_FLOW_MAPPING_VALUE_STATE:
            return yaml_emitter_emit_flow_mapping_value(emitter, event, 0);

        case YAML_EMIT_BLOCK_SEQUENCE_FIRST_ITEM_STATE:
            return yaml_emitter_emit_block_sequence_item(emitter, event, 1);

        case YAML_EMIT_BLOCK_SEQUENCE_ITEM_STATE:
            return yaml_emitter_emit_block_sequence_item(emitter, event, 0);

        case YAML_EMIT_BLOCK_MAPPING_FIRST_KEY_STATE:
            return yaml_emitter_emit_block_mapping_key(emitter, event, 1);

        case YAML_EMIT_BLOCK_MAPPING_KEY_STATE:
            return yaml_emitter_emit_block_mapping_key(emitter, event, 0);

        case YAML_EMIT_BLOCK_MAPPING_SIMPLE_VALUE_STATE:
            return yaml_emitter_emit_block_mapping_value(emitter, event, 1);

        case YAML_EMIT_BLOCK_MAPPING_VALUE_STATE:
            return yaml_emitter_emit_block_mapping_value(emitter, event, 0);

        case YAML_EMIT_END_STATE:
            return EMITTER_ERROR_INIT(emitter,
                    "expected nothing after STREAM-END");

        default:
            assert(1);      /* Invalid state. */
    }

    return 0;
}

/*
 * Expect STREAM-START.
 */

static int
yaml_emitter_emit_stream_start(yaml_emitter_t *emitter,
        yaml_event_t *event)
{
    if (event->type == YAML_STREAM_START_EVENT)
    {
        if (!emitter->encoding) {
            emitter->encoding = event->data.stream_start.encoding;
        }

        if (!emitter->encoding) {
            emitter->encoding = YAML_UTF8_ENCODING;
        }

        if (emitter->best_indent < 2 || emitter->best_indent > 9) {
            emitter->best_indent  = 2;
        }

        if (emitter->best_width >= 0
                && emitter->best_width <= emitter->best_indent*2) {
            emitter->best_width = 80;
        }

        if (emitter->best_width < 0) {
            emitter->best_width = INT_MAX;
        }
        
        if (!emitter->line_break) {
            emitter->line_break = YAML_LN_BREAK;
        }

        emitter->indent = -1;

        emitter->line = 0;
        emitter->column = 0;
        emitter->is_whitespace = 1;
        emitter->is_indention = 1;

        if (emitter->encoding != YAML_UTF8_ENCODING) {
            if (!yaml_emitter_write_bom(emitter))
                return 0;
        }

        emitter->state = YAML_EMIT_FIRST_DOCUMENT_START_STATE;

        return 1;
    }

    return EMITTER_ERROR_INIT(emitter, "expected STREAM-START");
}

/*
 * Expect DOCUMENT-START or STREAM-END.
 */

static int
yaml_emitter_emit_document_start(yaml_emitter_t *emitter,
        yaml_event_t *event, int first)
{
    if (event->type == YAML_DOCUMENT_START_EVENT)
    {
        yaml_tag_directive_t default_tag_directives[] = {
            {(yaml_char_t *)"!", (yaml_char_t *)"!"},
            {(yaml_char_t *)"!!", (yaml_char_t *)"tag:yaml.org,2002:"},
            {NULL, NULL}
        };
        yaml_tag_directive_t *tag_directive;
        int is_implicit;
        int idx;

        if (event->data.document_start.version_directive) {
            if (!yaml_emitter_analyze_version_directive(emitter,
                        *event->data.document_start.version_directive))
                return 0;
        }

        for (idx = 0; idx < event->data.document_start.tag_directives.length; idx++) {
            tag_directive = event->data.document_start.tag_directives.list+idx;
            if (!yaml_emitter_analyze_tag_directive(emitter, *tag_directive))
                return 0;
            if (!yaml_emitter_append_tag_directive(emitter, *tag_directive, 0))
                return 0;
        }

        for (tag_directive = default_tag_directives;
                tag_directive->handle; tag_directive ++) {
            if (!yaml_emitter_append_tag_directive(emitter, *tag_directive, 1))
                return 0;
        }

        is_implicit = event->data.document_start.is_implicit;
        if (!first || emitter->is_canonical) {
            is_implicit = 0;
        }

        if (event->data.document_start.version_directive) {
            is_implicit = 0;
            if (!yaml_emitter_write_indicator(emitter, "%YAML", 1, 0, 0))
                return 0;
            if (!yaml_emitter_write_indicator(emitter, "1.1", 1, 0, 0))
                return 0;
            if (!yaml_emitter_write_indent(emitter))
                return 0;
        }
        
        if (event->data.document_start.tag_directives.length) {
            is_implicit = 0;
            for (idx = 0; idx < event->data.document_start.tag_directives.length;
                    idx++) {
                tag_directive = event->data.document_start.tag_directives.list+idx;
                if (!yaml_emitter_write_indicator(emitter, "%TAG", 1, 0, 0))
                    return 0;
                if (!yaml_emitter_write_tag_handle(emitter, tag_directive->handle,
                            strlen((char *)tag_directive->handle)))
                    return 0;
                if (!yaml_emitter_write_tag_content(emitter, tag_directive->prefix,
                            strlen((char *)tag_directive->prefix), 1))
                    return 0;
                if (!yaml_emitter_write_indent(emitter))
                    return 0;
            }
        }

        if (yaml_emitter_check_empty_document(emitter)) {
            is_implicit = 0;
        }

        if (!is_implicit) {
            if (!yaml_emitter_write_indent(emitter))
                return 0;
            if (!yaml_emitter_write_indicator(emitter, "---", 1, 0, 0))
                return 0;
            if (emitter->is_canonical) {
                if (!yaml_emitter_write_indent(emitter))
                    return 0;
            }
        }

        emitter->state = YAML_EMIT_DOCUMENT_CONTENT_STATE;

        return 1;
    }

    else if (event->type == YAML_STREAM_END_EVENT)
    {
        if (!yaml_emitter_flush(emitter))
            return 0;

        emitter->state = YAML_EMIT_END_STATE;

        return 1;
    }

    return EMITTER_ERROR_INIT(emitter, "expected DOCUMENT-START or STREAM-END");
}

/*
 * Expect the root node.
 */

static int
yaml_emitter_emit_document_content(yaml_emitter_t *emitter,
        yaml_event_t *event)
{
    if (!PUSH(emitter, emitter->states, YAML_EMIT_DOCUMENT_END_STATE))
        return 0;

    return yaml_emitter_emit_node(emitter, event, 1, 0, 0, 0);
}

/*
 * Expect DOCUMENT-END.
 */

static int
yaml_emitter_emit_document_end(yaml_emitter_t *emitter,
        yaml_event_t *event)
{
    if (event->type == YAML_DOCUMENT_END_EVENT)
    {
        if (!yaml_emitter_write_indent(emitter))
            return 0;
        if (!event->data.document_end.is_implicit) {
            if (!yaml_emitter_write_indicator(emitter, "...", 1, 0, 0))
                return 0;
            if (!yaml_emitter_write_indent(emitter))
                return 0;
        }
        if (!yaml_emitter_flush(emitter))
            return 0;

        emitter->state = YAML_EMIT_DOCUMENT_START_STATE;

        while (!STACK_EMPTY(emitter, emitter->tag_directives)) {
            yaml_tag_directive_t tag_directive = POP(emitter,
                    emitter->tag_directives);
            yaml_free(tag_directive.handle);
            yaml_free(tag_directive.prefix);
        }

        return 1;
    }

    return EMITTER_ERROR_INIT(emitter, "expected DOCUMENT-END");
}

/*
 * 
 * Expect a flow item node.
 */

static int
yaml_emitter_emit_flow_sequence_item(yaml_emitter_t *emitter,
        yaml_event_t *event, int first)
{
    if (first)
    {
        if (!yaml_emitter_write_indicator(emitter, "[", 1, 1, 0))
            return 0;
        if (!yaml_emitter_increase_indent(emitter, 1, 0))
            return 0;
        emitter->flow_level ++;
    }

    if (event->type == YAML_SEQUENCE_END_EVENT)
    {
        emitter->flow_level --;
        emitter->indent = POP(emitter, emitter->indents);
        if (emitter->is_canonical && !first) {
            if (!yaml_emitter_write_indicator(emitter, ",", 0, 0, 0))
                return 0;
            if (!yaml_emitter_write_indent(emitter))
                return 0;
        }
        if (!yaml_emitter_write_indicator(emitter, "]", 0, 0, 0))
            return 0;
        emitter->state = POP(emitter, emitter->states);

        return 1;
    }

    if (!first) {
        if (!yaml_emitter_write_indicator(emitter, ",", 0, 0, 0))
            return 0;
    }

    if (emitter->is_canonical || emitter->column > emitter->best_width) {
        if (!yaml_emitter_write_indent(emitter))
            return 0;
    }
    if (!PUSH(emitter, emitter->states, YAML_EMIT_FLOW_SEQUENCE_ITEM_STATE))
        return 0;

    return yaml_emitter_emit_node(emitter, event, 0, 1, 0, 0);
}

/*
 * Expect a flow key node.
 */

static int
yaml_emitter_emit_flow_mapping_key(yaml_emitter_t *emitter,
        yaml_event_t *event, int first)
{
    if (first)
    {
        if (!yaml_emitter_write_indicator(emitter, "{", 1, 1, 0))
            return 0;
        if (!yaml_emitter_increase_indent(emitter, 1, 0))
            return 0;
        emitter->flow_level ++;
    }

    if (event->type == YAML_MAPPING_END_EVENT)
    {
        emitter->flow_level --;
        emitter->indent = POP(emitter, emitter->indents);
        if (emitter->is_canonical && !first) {
            if (!yaml_emitter_write_indicator(emitter, ",", 0, 0, 0))
                return 0;
            if (!yaml_emitter_write_indent(emitter))
                return 0;
        }
        if (!yaml_emitter_write_indicator(emitter, "}", 0, 0, 0))
            return 0;
        emitter->state = POP(emitter, emitter->states);

        return 1;
    }

    if (!first) {
        if (!yaml_emitter_write_indicator(emitter, ",", 0, 0, 0))
            return 0;
    }
    if (emitter->is_canonical || emitter->column > emitter->best_width) {
        if (!yaml_emitter_write_indent(emitter))
            return 0;
    }

    if (!emitter->is_canonical && yaml_emitter_check_simple_key(emitter))
    {
        if (!PUSH(emitter, emitter->states,
                    YAML_EMIT_FLOW_MAPPING_SIMPLE_VALUE_STATE))
            return 0;

        return yaml_emitter_emit_node(emitter, event, 0, 0, 1, 1);
    }
    else
    {
        if (!yaml_emitter_write_indicator(emitter, "?", 1, 0, 0))
            return 0;
        if (!PUSH(emitter, emitter->states,
                    YAML_EMIT_FLOW_MAPPING_VALUE_STATE))
            return 0;

        return yaml_emitter_emit_node(emitter, event, 0, 0, 1, 0);
    }
}

/*
 * Expect a flow value node.
 */

static int
yaml_emitter_emit_flow_mapping_value(yaml_emitter_t *emitter,
        yaml_event_t *event, int simple)
{
    if (simple) {
        if (!yaml_emitter_write_indicator(emitter, ":", 0, 0, 0))
            return 0;
    }
    else {
        if (emitter->is_canonical || emitter->column > emitter->best_width) {
            if (!yaml_emitter_write_indent(emitter))
                return 0;
        }
        if (!yaml_emitter_write_indicator(emitter, ":", 1, 0, 0))
            return 0;
    }
    if (!PUSH(emitter, emitter->states, YAML_EMIT_FLOW_MAPPING_KEY_STATE))
        return 0;
    return yaml_emitter_emit_node(emitter, event, 0, 0, 1, 0);
}

/*
 * Expect a block item node.
 */

static int
yaml_emitter_emit_block_sequence_item(yaml_emitter_t *emitter,
        yaml_event_t *event, int first)
{
    if (first)
    {
        if (!yaml_emitter_increase_indent(emitter, 0,
                    (emitter->is_mapping_context && !emitter->is_indention)))
            return 0;
    }

    if (event->type == YAML_SEQUENCE_END_EVENT)
    {
        emitter->indent = POP(emitter, emitter->indents);
        emitter->state = POP(emitter, emitter->states);

        return 1;
    }

    if (!yaml_emitter_write_indent(emitter))
        return 0;
    if (!yaml_emitter_write_indicator(emitter, "-", 1, 0, 1))
        return 0;
    if (!PUSH(emitter, emitter->states,
                YAML_EMIT_BLOCK_SEQUENCE_ITEM_STATE))
        return 0;

    return yaml_emitter_emit_node(emitter, event, 0, 1, 0, 0);
}

/*
 * Expect a block key node.
 */

static int
yaml_emitter_emit_block_mapping_key(yaml_emitter_t *emitter,
        yaml_event_t *event, int first)
{
    if (first)
    {
        if (!yaml_emitter_increase_indent(emitter, 0, 0))
            return 0;
    }

    if (event->type == YAML_MAPPING_END_EVENT)
    {
        emitter->indent = POP(emitter, emitter->indents);
        emitter->state = POP(emitter, emitter->states);

        return 1;
    }

    if (!yaml_emitter_write_indent(emitter))
        return 0;

    if (yaml_emitter_check_simple_key(emitter))
    {
        if (!PUSH(emitter, emitter->states,
                    YAML_EMIT_BLOCK_MAPPING_SIMPLE_VALUE_STATE))
            return 0;

        return yaml_emitter_emit_node(emitter, event, 0, 0, 1, 1);
    }
    else
    {
        if (!yaml_emitter_write_indicator(emitter, "?", 1, 0, 1))
            return 0;
        if (!PUSH(emitter, emitter->states,
                    YAML_EMIT_BLOCK_MAPPING_VALUE_STATE))
            return 0;

        return yaml_emitter_emit_node(emitter, event, 0, 0, 1, 0);
    }
}

/*
 * Expect a block value node.
 */

static int
yaml_emitter_emit_block_mapping_value(yaml_emitter_t *emitter,
        yaml_event_t *event, int simple)
{
    if (simple) {
        if (!yaml_emitter_write_indicator(emitter, ":", 0, 0, 0))
            return 0;
    }
    else {
        if (!yaml_emitter_write_indent(emitter))
            return 0;
        if (!yaml_emitter_write_indicator(emitter, ":", 1, 0, 1))
            return 0;
    }
    if (!PUSH(emitter, emitter->states,
                YAML_EMIT_BLOCK_MAPPING_KEY_STATE))
        return 0;

    return yaml_emitter_emit_node(emitter, event, 0, 0, 1, 0);
}

/*
 * Expect a node.
 */

static int
yaml_emitter_emit_node(yaml_emitter_t *emitter, yaml_event_t *event,
        int is_root, int is_sequence, int is_mapping, int is_simple_key)
{
    emitter->is_root_context = is_root;
    emitter->is_sequence_context = is_sequence;
    emitter->is_mapping_context = is_mapping;
    emitter->is_simple_key_context = is_simple_key;

    switch (event->type)
    {
        case YAML_ALIAS_EVENT:
            return yaml_emitter_emit_alias(emitter, event);

        case YAML_SCALAR_EVENT:
            return yaml_emitter_emit_scalar(emitter, event);

        case YAML_SEQUENCE_START_EVENT:
            return yaml_emitter_emit_sequence_start(emitter, event);

        case YAML_MAPPING_START_EVENT:
            return yaml_emitter_emit_mapping_start(emitter, event);

        default:
            return EMITTER_ERROR_INIT(emitter,
                    "expected SCALAR, SEQUENCE-START, MAPPING-START, or ALIAS");
    }

    return 0;
}

/*
 * Expect ALIAS.
 */

static int
yaml_emitter_emit_alias(yaml_emitter_t *emitter, yaml_event_t *event)
{
    if (!yaml_emitter_process_anchor(emitter))
        return 0;
    emitter->state = POP(emitter, emitter->states);

    return 1;
}

/*
 * Expect SCALAR.
 */

static int
yaml_emitter_emit_scalar(yaml_emitter_t *emitter, yaml_event_t *event)
{
    if (!yaml_emitter_select_scalar_style(emitter, event))
        return 0;
    if (!yaml_emitter_process_anchor(emitter))
        return 0;
    if (!yaml_emitter_process_tag(emitter))
        return 0;
    if (!yaml_emitter_increase_indent(emitter, 1, 0))
        return 0;
    if (!yaml_emitter_process_scalar(emitter))
        return 0;
    emitter->indent = POP(emitter, emitter->indents);
    emitter->state = POP(emitter, emitter->states);

    return 1;
}

/*
 * Expect SEQUENCE-START.
 */

static int
yaml_emitter_emit_sequence_start(yaml_emitter_t *emitter, yaml_event_t *event)
{
    if (!yaml_emitter_process_anchor(emitter))
        return 0;
    if (!yaml_emitter_process_tag(emitter))
        return 0;

    if (emitter->flow_level || emitter->is_canonical
            || event->data.sequence_start.style == YAML_FLOW_SEQUENCE_STYLE
            || yaml_emitter_check_empty_sequence(emitter)) {
        emitter->state = YAML_EMIT_FLOW_SEQUENCE_FIRST_ITEM_STATE;
    }
    else {
        emitter->state = YAML_EMIT_BLOCK_SEQUENCE_FIRST_ITEM_STATE;
    }

    return 1;
}

/*
 * Expect MAPPING-START.
 */

static int
yaml_emitter_emit_mapping_start(yaml_emitter_t *emitter, yaml_event_t *event)
{
    if (!yaml_emitter_process_anchor(emitter))
        return 0;
    if (!yaml_emitter_process_tag(emitter))
        return 0;

    if (emitter->flow_level || emitter->is_canonical
            || event->data.mapping_start.style == YAML_FLOW_MAPPING_STYLE
            || yaml_emitter_check_empty_mapping(emitter)) {
        emitter->state = YAML_EMIT_FLOW_MAPPING_FIRST_KEY_STATE;
    }
    else {
        emitter->state = YAML_EMIT_BLOCK_MAPPING_FIRST_KEY_STATE;
    }

    return 1;
}

/*
 * Check if the document content is an empty scalar.
 */

static int
yaml_emitter_check_empty_document(yaml_emitter_t *emitter)
{
    return 0;
}

/*
 * Check if the next events represent an empty sequence.
 */

static int
yaml_emitter_check_empty_sequence(yaml_emitter_t *emitter)
{
    if (emitter->events.tail - emitter->events.head < 2)
        return 0;

    return (emitter->events.list[emitter->events.head].type
                            == YAML_SEQUENCE_START_EVENT &&
            emitter->events.list[emitter->events.head+1].type
                            == YAML_SEQUENCE_END_EVENT);
}

/*
 * Check if the next events represent an empty mapping.
 */

static int
yaml_emitter_check_empty_mapping(yaml_emitter_t *emitter)
{
    if (emitter->events.tail - emitter->events.head < 2)
        return 0;

    return (emitter->events.list[emitter->events.head].type
                            == YAML_MAPPING_START_EVENT &&
            emitter->events.list[emitter->events.head+1].type
                            == YAML_MAPPING_END_EVENT);
}

/*
 * Check if the next node can be expressed as a simple key.
 */

static int
yaml_emitter_check_simple_key(yaml_emitter_t *emitter)
{
    yaml_event_t *event = emitter->events.list + emitter->events.head;
    size_t length = 0;

    switch (event->type)
    {
        case YAML_ALIAS_EVENT:
            length += emitter->anchor_data.anchor_length;
            break;

        case YAML_SCALAR_EVENT:
            if (emitter->scalar_data.is_multiline)
                return 0;
            length += emitter->anchor_data.anchor_length
                + emitter->tag_data.handle_length
                + emitter->tag_data.suffix_length
                + emitter->scalar_data.length;
            break;

        case YAML_SEQUENCE_START_EVENT:
            if (!yaml_emitter_check_empty_sequence(emitter))
                return 0;
            length += emitter->anchor_data.anchor_length
                + emitter->tag_data.handle_length
                + emitter->tag_data.suffix_length;
            break;

        case YAML_MAPPING_START_EVENT:
            if (!yaml_emitter_check_empty_sequence(emitter))
                return 0;
            length += emitter->anchor_data.anchor_length
                + emitter->tag_data.handle_length
                + emitter->tag_data.suffix_length;
            break;

        default:
            return 0;
    }

    if (length > 128)
        return 0;

    return 1;
}

/*
 * Determine an acceptable scalar style.
 */

static int
yaml_emitter_select_scalar_style(yaml_emitter_t *emitter, yaml_event_t *event)
{
    yaml_scalar_style_t style = event->data.scalar.style;
    int no_tag = (!emitter->tag_data.handle && !emitter->tag_data.suffix);

    if (no_tag && !event->data.scalar.is_plain_implicit
            && !event->data.scalar.is_quoted_implicit) {
        return EMITTER_ERROR_INIT(emitter,
                "neither tag nor implicit flags are specified");
    }

    if (style == YAML_ANY_SCALAR_STYLE)
        style = YAML_PLAIN_SCALAR_STYLE;

    if (emitter->is_canonical)
        style = YAML_DOUBLE_QUOTED_SCALAR_STYLE;

    if (emitter->is_simple_key_context && emitter->scalar_data.is_multiline)
        style = YAML_DOUBLE_QUOTED_SCALAR_STYLE;

    if (style == YAML_PLAIN_SCALAR_STYLE)
    {
        if ((emitter->flow_level && !emitter->scalar_data.is_flow_plain_allowed)
                || (!emitter->flow_level && !emitter->scalar_data.is_block_plain_allowed))
            style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
        if (!emitter->scalar_data.length
                && (emitter->flow_level || emitter->is_simple_key_context))
            style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
        if (no_tag && !event->data.scalar.is_plain_implicit)
            style = YAML_SINGLE_QUOTED_SCALAR_STYLE;
    }

    if (style == YAML_SINGLE_QUOTED_SCALAR_STYLE)
    {
        if (!emitter->scalar_data.is_single_quoted_allowed)
            style = YAML_DOUBLE_QUOTED_SCALAR_STYLE;
    }

    if (style == YAML_LITERAL_SCALAR_STYLE || style == YAML_FOLDED_SCALAR_STYLE)
    {
        if (!emitter->scalar_data.is_block_allowed
                || emitter->flow_level || emitter->is_simple_key_context)
            style = YAML_DOUBLE_QUOTED_SCALAR_STYLE;
    }

    if (no_tag && !event->data.scalar.is_quoted_implicit
            && style != YAML_PLAIN_SCALAR_STYLE)
    {
        emitter->tag_data.handle = (yaml_char_t *)"!";
        emitter->tag_data.handle_length = 1;
    }

    emitter->scalar_data.style = style;

    return 1;
}

/*
 * Write an achor.
 */

static int
yaml_emitter_process_anchor(yaml_emitter_t *emitter)
{
    if (!emitter->anchor_data.anchor)
        return 1;

    if (!yaml_emitter_write_indicator(emitter,
                (emitter->anchor_data.is_alias ? "*" : "&"), 1, 0, 0))
        return 0;

    return yaml_emitter_write_anchor(emitter,
            emitter->anchor_data.anchor, emitter->anchor_data.anchor_length);
}

/*
 * Write a tag.
 */

static int
yaml_emitter_process_tag(yaml_emitter_t *emitter)
{
    if (!emitter->tag_data.handle && !emitter->tag_data.suffix)
        return 1;

    if (emitter->tag_data.handle)
    {
        if (!yaml_emitter_write_tag_handle(emitter, emitter->tag_data.handle,
                    emitter->tag_data.handle_length))
            return 0;
        if (emitter->tag_data.suffix) {
            if (!yaml_emitter_write_tag_content(emitter, emitter->tag_data.suffix,
                        emitter->tag_data.suffix_length, 0))
                return 0;
        }
    }
    else
    {
        if (!yaml_emitter_write_indicator(emitter, "!<", 1, 0, 0))
            return 0;
        if (!yaml_emitter_write_tag_content(emitter, emitter->tag_data.suffix,
                    emitter->tag_data.suffix_length, 0))
            return 0;
        if (!yaml_emitter_write_indicator(emitter, ">", 0, 0, 0))
            return 0;
    }

    return 1;
}

/*
 * Write a scalar.
 */

static int
yaml_emitter_process_scalar(yaml_emitter_t *emitter)
{
    switch (emitter->scalar_data.style)
    {
        case YAML_PLAIN_SCALAR_STYLE:
            return yaml_emitter_write_plain_scalar(emitter,
                    emitter->scalar_data.value, emitter->scalar_data.length,
                    !emitter->is_simple_key_context);

        case YAML_SINGLE_QUOTED_SCALAR_STYLE:
            return yaml_emitter_write_single_quoted_scalar(emitter,
                    emitter->scalar_data.value, emitter->scalar_data.length,
                    !emitter->is_simple_key_context);

        case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
            return yaml_emitter_write_double_quoted_scalar(emitter,
                    emitter->scalar_data.value, emitter->scalar_data.length,
                    !emitter->is_simple_key_context);

        case YAML_LITERAL_SCALAR_STYLE:
            return yaml_emitter_write_literal_scalar(emitter,
                    emitter->scalar_data.value, emitter->scalar_data.length);

        case YAML_FOLDED_SCALAR_STYLE:
            return yaml_emitter_write_folded_scalar(emitter,
                    emitter->scalar_data.value, emitter->scalar_data.length);

        default:
            assert(1);      /* Impossible. */
    }

    return 0;
}

/*
 * Check if a %YAML directive is valid.
 */

static int
yaml_emitter_analyze_version_directive(yaml_emitter_t *emitter,
        yaml_version_directive_t version_directive)
{
    if (version_directive.major != 1 || version_directive.minor != 1) {
        return EMITTER_ERROR_INIT(emitter, "incompatible %YAML directive");
    }

    return 1;
}

/*
 * Check if a %TAG directive is valid.
 */

static int
yaml_emitter_analyze_tag_directive(yaml_emitter_t *emitter,
        yaml_tag_directive_t tag_directive)
{
    yaml_string_t handle = STRING(tag_directive.handle,
            strlen((char *)tag_directive.handle));
    yaml_string_t prefix = STRING(tag_directive.prefix,
            strlen((char *)tag_directive.prefix));

    if (!handle.capacity) {
        return EMITTER_ERROR_INIT(emitter, "tag handle must not be empty");
    }

    if (handle.buffer[0] != '!') {
        return EMITTER_ERROR_INIT(emitter, "tag handle must start with '!'");
    }

    if (handle.buffer[handle.capacity-1] != '!') {
        return EMITTER_ERROR_INIT(emitter, "tag handle must end with '!'");
    }

    handle.pointer ++;

    while (handle.pointer < handle.capacity-1) {
        if (!IS_ALPHA(handle)) {
            return EMITTER_ERROR_INIT(emitter,
                    "tag handle must contain alphanumerical characters only");
        }
        MOVE(handle);
    }

    if (!prefix.capacity) {
        return EMITTER_ERROR_INIT(emitter, "tag prefix must not be empty");
    }

    return 1;
}

/*
 * Check if an anchor is valid.
 */

static int
yaml_emitter_analyze_anchor(yaml_emitter_t *emitter,
        yaml_char_t *anchor, int is_alias)
{
    yaml_string_t string = STRING(anchor, strlen((char *)anchor));

    if (!string.capacity) {
        return EMITTER_ERROR_INIT(emitter, is_alias ?
                "alias value must not be empty" :
                "anchor value must not be empty");
    }

    while (string.pointer < string.capacity) {
        if (!IS_ALPHA(string)) {
            return EMITTER_ERROR_INIT(emitter, is_alias ?
                    "alias value must contain alphanumerical characters only" :
                    "anchor value must contain alphanumerical characters only");
        }
        MOVE(string);
    }

    emitter->anchor_data.anchor = string.buffer;
    emitter->anchor_data.anchor_length = string.capacity;
    emitter->anchor_data.is_alias = is_alias;

    return 1;
}

/*
 * Check if a tag is valid.
 */

static int
yaml_emitter_analyze_tag(yaml_emitter_t *emitter,
        yaml_char_t *tag)
{
    yaml_string_t string = STRING(tag, strlen((char *)tag));
    size_t idx;

    if (!string.capacity) {
        return EMITTER_ERROR_INIT(emitter, "tag value must not be empty");
    }

    for (idx = 0; idx < emitter->tag_directives.length; idx ++) {
        yaml_tag_directive_t *tag_directive = emitter->tag_directives.list+idx;
        size_t prefix_length = strlen((char *)tag_directive->prefix);
        if (prefix_length < string.capacity
                && strncmp((char *)tag_directive->prefix, (char *)string.buffer,
                    prefix_length) == 0)
        {
            emitter->tag_data.handle = tag_directive->handle;
            emitter->tag_data.handle_length =
                strlen((char *)tag_directive->handle);
            emitter->tag_data.suffix = string.buffer + prefix_length;
            emitter->tag_data.suffix_length = string.capacity - prefix_length;
            return 1;
        }
    }

    emitter->tag_data.suffix = string.buffer;
    emitter->tag_data.suffix_length = string.capacity;

    return 1;
}

/*
 * Check if a scalar is valid.
 */

static int
yaml_emitter_analyze_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length)
{
    yaml_string_t string = STRING(value, length);

    int block_indicators = 0;
    int flow_indicators = 0;
    int line_breaks = 0;
    int special_characters = 0;

    int inline_spaces = 0;
    int inline_breaks = 0;
    int leading_spaces = 0;
    int leading_breaks = 0;
    int trailing_spaces = 0;
    int trailing_breaks = 0;
    int inline_breaks_spaces = 0;
    int mixed_breaks_spaces = 0;

    int preceeded_by_space = 0;
    int followed_by_space = 0;
    int spaces = 0;
    int breaks = 0;
    int mixed = 0;
    int leading = 0;

    emitter->scalar_data.value = value;
    emitter->scalar_data.length = length;

    if (!string.capacity)
    {
        emitter->scalar_data.is_multiline = 0;
        emitter->scalar_data.is_flow_plain_allowed = 0;
        emitter->scalar_data.is_block_plain_allowed = 1;
        emitter->scalar_data.is_single_quoted_allowed = 1;
        emitter->scalar_data.is_block_allowed = 0;

        return 1;
    }

    if ((CHECK_AT(string, '-', 0)
                && CHECK_AT(string, '-', 1)
                && CHECK_AT(string, '-', 2))
            || (CHECK_AT(string, '.', 0)
                && CHECK_AT(string, '.', 1)
                && CHECK_AT(string, '.', 2))) {
        block_indicators = 1;
        flow_indicators = 1;
    }

    preceeded_by_space = 1;
    followed_by_space = IS_BLANKZ_AT(string, WIDTH(string));

    while (string.pointer < string.capacity)
    {
        if (!string.pointer)
        {
            if (CHECK(string, '#') || CHECK(string, ',')
                    || CHECK(string, '[') || CHECK(string, ']')
                    || CHECK(string, '{') || CHECK(string, '}')
                    || CHECK(string, '&') || CHECK(string, '*')
                    || CHECK(string, '!') || CHECK(string, '|')
                    || CHECK(string, '>') || CHECK(string, '\'')
                    || CHECK(string, '"') || CHECK(string, '%')
                    || CHECK(string, '@') || CHECK(string, '`')) {
                flow_indicators = 1;
                block_indicators = 1;
            }

            if (CHECK(string, '?') || CHECK(string, ':')) {
                flow_indicators = 1;
                if (followed_by_space) {
                    block_indicators = 1;
                }
            }

            if (CHECK(string, '-') && followed_by_space) {
                flow_indicators = 1;
                block_indicators = 1;
            }
        }
        else
        {
            if (CHECK(string, ',') || CHECK(string, '?')
                    || CHECK(string, '[') || CHECK(string, ']')
                    || CHECK(string, '{') || CHECK(string, '}')) {
                flow_indicators = 1;
            }

            if (CHECK(string, ':')) {
                flow_indicators = 1;
                if (followed_by_space) {
                    block_indicators = 1;
                }
            }

            if (CHECK(string, '#') && preceeded_by_space) {
                flow_indicators = 1;
                block_indicators = 1;
            }
        }

        if (!IS_PRINTABLE(string)
                || (!IS_ASCII(string) && !emitter->is_unicode)) {
            special_characters = 1;
        }

        if (IS_BREAK(string)) {
            line_breaks = 1;
        }

        if (IS_SPACE(string))
        {
            spaces = 1;
            if (!string.pointer) {
                leading = 1;
            }
        }

        else if (IS_BREAK(string))
        {
            if (spaces) {
                mixed = 1;
            }
            breaks = 1;
            if (!string.pointer) {
                leading = 1;
            }
        }

        else if (spaces || breaks)
        {
            if (leading) {
                if (spaces && breaks) {
                    mixed_breaks_spaces = 1;
                }
                else if (spaces) {
                    leading_spaces = 1;
                }
                else if (breaks) {
                    leading_breaks = 1;
                }
            }
            else {
                if (mixed) {
                    mixed_breaks_spaces = 1;
                }
                else if (spaces && breaks) {
                    inline_breaks_spaces = 1;
                }
                else if (spaces) {
                    inline_spaces = 1;
                }
                else if (breaks) {
                    inline_breaks = 1;
                }
            }
            spaces = breaks = mixed = leading = 0;
        }

        if ((spaces || breaks) && string.pointer == string.capacity-1)
        {
            if (spaces && breaks) {
                mixed_breaks_spaces = 1;
            }
            else if (spaces) {
                if (leading) {
                    leading_spaces = 1;
                }
                trailing_spaces = 1;
            }
            else if (breaks) {
                if (leading) {
                    leading_breaks = 1;
                }
                trailing_breaks = 1;
            }
        }

        preceeded_by_space = IS_BLANKZ(string);
        MOVE(string);
        if (string.pointer < string.capacity) {
            followed_by_space = IS_BLANKZ_AT(string, WIDTH(string));
        }
    }

    emitter->scalar_data.is_multiline = line_breaks;

    emitter->scalar_data.is_flow_plain_allowed = 1;
    emitter->scalar_data.is_block_plain_allowed = 1;
    emitter->scalar_data.is_single_quoted_allowed = 1;
    emitter->scalar_data.is_block_allowed = 1;

    if (leading_spaces || leading_breaks || trailing_spaces) {
        emitter->scalar_data.is_flow_plain_allowed = 0;
        emitter->scalar_data.is_block_plain_allowed = 0;
        emitter->scalar_data.is_block_allowed = 0;
    }

    if (trailing_breaks) {
        emitter->scalar_data.is_flow_plain_allowed = 0;
        emitter->scalar_data.is_block_plain_allowed = 0;
    }

    if (inline_breaks_spaces) {
        emitter->scalar_data.is_flow_plain_allowed = 0;
        emitter->scalar_data.is_block_plain_allowed = 0;
        emitter->scalar_data.is_single_quoted_allowed = 0;
    }

    if (mixed_breaks_spaces || special_characters) {
        emitter->scalar_data.is_flow_plain_allowed = 0;
        emitter->scalar_data.is_block_plain_allowed = 0;
        emitter->scalar_data.is_single_quoted_allowed = 0;
        emitter->scalar_data.is_block_allowed = 0;
    }

    if (line_breaks) {
        emitter->scalar_data.is_flow_plain_allowed = 0;
        emitter->scalar_data.is_block_plain_allowed = 0;
    }

    if (flow_indicators) {
        emitter->scalar_data.is_flow_plain_allowed = 0;
    }

    if (block_indicators) {
        emitter->scalar_data.is_block_plain_allowed = 0;
    }

    return 1;
}

/*
 * Check if the event data is valid.
 */

static int
yaml_emitter_analyze_event(yaml_emitter_t *emitter,
        yaml_event_t *event)
{
    emitter->anchor_data.anchor = NULL;
    emitter->anchor_data.anchor_length = 0;
    emitter->tag_data.handle = NULL;
    emitter->tag_data.handle_length = 0;
    emitter->tag_data.suffix = NULL;
    emitter->tag_data.suffix_length = 0;
    emitter->scalar_data.value = NULL;
    emitter->scalar_data.length = 0;

    switch (event->type)
    {
        case YAML_ALIAS_EVENT:
            if (!yaml_emitter_analyze_anchor(emitter,
                        event->data.alias.anchor, 1))
                return 0;
            return 1;

        case YAML_SCALAR_EVENT:
            if (event->data.scalar.anchor) {
                if (!yaml_emitter_analyze_anchor(emitter,
                            event->data.scalar.anchor, 0))
                    return 0;
            }
            if (event->data.scalar.tag && (emitter->is_canonical ||
                        (!event->data.scalar.is_plain_implicit
                         && !event->data.scalar.is_quoted_implicit))) {
                if (!yaml_emitter_analyze_tag(emitter, event->data.scalar.tag))
                    return 0;
            }
            if (!yaml_emitter_analyze_scalar(emitter,
                        event->data.scalar.value, event->data.scalar.length))
                return 0;
            return 1;

        case YAML_SEQUENCE_START_EVENT:
            if (event->data.sequence_start.anchor) {
                if (!yaml_emitter_analyze_anchor(emitter,
                            event->data.sequence_start.anchor, 0))
                    return 0;
            }
            if (event->data.sequence_start.tag && (emitter->is_canonical ||
                        !event->data.sequence_start.is_implicit)) {
                if (!yaml_emitter_analyze_tag(emitter,
                            event->data.sequence_start.tag))
                    return 0;
            }
            return 1;

        case YAML_MAPPING_START_EVENT:
            if (event->data.mapping_start.anchor) {
                if (!yaml_emitter_analyze_anchor(emitter,
                            event->data.mapping_start.anchor, 0))
                    return 0;
            }
            if (event->data.mapping_start.tag && (emitter->is_canonical ||
                        !event->data.mapping_start.is_implicit)) {
                if (!yaml_emitter_analyze_tag(emitter,
                            event->data.mapping_start.tag))
                    return 0;
            }
            return 1;

        default:
            return 1;
    }
}

/*
 * Write the BOM character.
 */

static int
yaml_emitter_write_bom(yaml_emitter_t *emitter)
{
    if (!FLUSH(emitter)) return 0;

    JOIN_OCTET(emitter->output, (yaml_char_t) '\xEF');
    JOIN_OCTET(emitter->output, (yaml_char_t) '\xBB');
    JOIN_OCTET(emitter->output, (yaml_char_t) '\xBF');

    return 1;
}

static int
yaml_emitter_write_indent(yaml_emitter_t *emitter)
{
    int indent = (emitter->indent >= 0) ? emitter->indent : 0;

    if (!emitter->is_indention || emitter->column > indent
            || (emitter->column == indent && !emitter->is_whitespace)) {
        if (!PUT_BREAK(emitter)) return 0;
    }

    while (emitter->column < indent) {
        if (!PUT(emitter, ' ')) return 0;
    }

    emitter->is_whitespace = 1;
    emitter->is_indention = 1;

    return 1;
}

static int
yaml_emitter_write_indicator(yaml_emitter_t *emitter,
        char *indicator, int need_whitespace,
        int is_whitespace, int is_indention)
{
    yaml_string_t string = STRING((yaml_char_t *)indicator, strlen(indicator));

    if (need_whitespace && !emitter->is_whitespace) {
        if (!PUT(emitter, ' ')) return 0;
    }

    while (string.pointer < string.capacity) {
        if (!WRITE(emitter, string)) return 0;
    }

    emitter->is_whitespace = is_whitespace;
    emitter->is_indention = (emitter->is_indention && is_indention);

    return 1;
}

static int
yaml_emitter_write_anchor(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length)
{
    yaml_string_t string = STRING(value, length);

    while (string.pointer < string.capacity) {
        if (!WRITE(emitter, string)) return 0;
    }

    emitter->is_whitespace = 0;
    emitter->is_indention = 0;

    return 1;
}

static int
yaml_emitter_write_tag_handle(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length)
{
    yaml_string_t string = STRING(value, length);

    if (!emitter->is_whitespace) {
        if (!PUT(emitter, ' ')) return 0;
    }

    while (string.pointer < string.capacity) {
        if (!WRITE(emitter, string)) return 0;
    }

    emitter->is_whitespace = 0;
    emitter->is_indention = 0;

    return 1;
}

static int
yaml_emitter_write_tag_content(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length,
        int need_whitespace)
{
    yaml_string_t string = STRING(value, length);

    if (need_whitespace && !emitter->is_whitespace) {
        if (!PUT(emitter, ' ')) return 0;
    }

    while (string.pointer < string.capacity) {
        if (IS_ALPHA(string)
                || CHECK(string, ';') || CHECK(string, '/')
                || CHECK(string, '?') || CHECK(string, ':')
                || CHECK(string, '@') || CHECK(string, '&')
                || CHECK(string, '=') || CHECK(string, '+')
                || CHECK(string, '$') || CHECK(string, ',')
                || CHECK(string, '_') || CHECK(string, '.')
                || CHECK(string, '~') || CHECK(string, '*')
                || CHECK(string, '\'') || CHECK(string, '(')
                || CHECK(string, ')') || CHECK(string, '[')
                || CHECK(string, ']')) {
            if (!WRITE(emitter, string)) return 0;
        }
        else {
            int width = WIDTH(string);
            unsigned int value;
            while (width --) {
                value = OCTET(string);
                string.pointer ++;
                if (!PUT(emitter, '%')) return 0;
                if (!PUT(emitter, (value >> 4)
                            + ((value >> 4) < 10 ? '0' : 'A' - 10)))
                    return 0;
                if (!PUT(emitter, (value & 0x0F)
                            + ((value & 0x0F) < 10 ? '0' : 'A' - 10)))
                    return 0;
            }
        }
    }

    emitter->is_whitespace = 0;
    emitter->is_indention = 0;

    return 1;
}

static int
yaml_emitter_write_plain_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int allow_breaks)
{
    yaml_string_t string = STRING(value, length);
    int spaces = 0;
    int breaks = 0;

    if (!emitter->is_whitespace) {
        if (!PUT(emitter, ' ')) return 0;
    }

    while (string.pointer < string.capacity)
    {
        if (IS_SPACE(string))
        {
            if (allow_breaks && !spaces
                    && emitter->column > emitter->best_width
                    && !IS_SPACE_AT(string, 1)) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
                MOVE(string);
            }
            else {
                if (!WRITE(emitter, string)) return 0;
            }
            spaces = 1;
        }
        else if (IS_BREAK(string))
        {
            if (!breaks && CHECK(string, '\n')) {
                if (!PUT_BREAK(emitter)) return 0;
            }
            if (!WRITE_BREAK(emitter, string)) return 0;
            emitter->is_indention = 1;
            breaks = 1;
        }
        else
        {
            if (breaks) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
            }
            if (!WRITE(emitter, string)) return 0;
            emitter->is_indention = 0;
            spaces = 0;
            breaks = 0;
        }
    }

    emitter->is_whitespace = 0;
    emitter->is_indention = 0;

    return 1;
}

static int
yaml_emitter_write_single_quoted_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int allow_breaks)
{
    yaml_string_t string = STRING(value, length);
    int spaces = 0;
    int breaks = 0;

    if (!yaml_emitter_write_indicator(emitter, "'", 1, 0, 0))
        return 0;

    while (string.pointer < string.capacity)
    {
        if (IS_SPACE(string))
        {
            if (allow_breaks && !spaces
                    && emitter->column > emitter->best_width
                    && string.pointer != 0
                    && string.pointer != string.capacity - 1
                    && !IS_SPACE_AT(string, 1)) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
                MOVE(string);
            }
            else {
                if (!WRITE(emitter, string)) return 0;
            }
            spaces = 1;
        }
        else if (IS_BREAK(string))
        {
            if (!breaks && CHECK(string, '\n')) {
                if (!PUT_BREAK(emitter)) return 0;
            }
            if (!WRITE_BREAK(emitter, string)) return 0;
            emitter->is_indention = 1;
            breaks = 1;
        }
        else
        {
            if (breaks) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
            }
            if (CHECK(string, '\'')) {
                if (!PUT(emitter, '\'')) return 0;
            }
            if (!WRITE(emitter, string)) return 0;
            emitter->is_indention = 0;
            spaces = 0;
            breaks = 0;
        }
    }

    if (!yaml_emitter_write_indicator(emitter, "'", 0, 0, 0))
        return 0;

    emitter->is_whitespace = 0;
    emitter->is_indention = 0;

    return 1;
}

static int
yaml_emitter_write_double_quoted_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length, int allow_breaks)
{
    yaml_string_t string = STRING(value, length);
    int spaces = 0;

    if (!yaml_emitter_write_indicator(emitter, "\"", 1, 0, 0))
        return 0;

    while (string.pointer < string.capacity)
    {
        if (!IS_PRINTABLE(string) || (!emitter->is_unicode && !IS_ASCII(string))
                || IS_BOM(string) || IS_BREAK(string)
                || CHECK(string, '"') || CHECK(string, '\\'))
        {
            unsigned char octet;
            unsigned int width;
            unsigned int value;
            int idx;

            octet = OCTET(string);
            width = (octet & 0x80) == 0x00 ? 1 :
                    (octet & 0xE0) == 0xC0 ? 2 :
                    (octet & 0xF0) == 0xE0 ? 3 :
                    (octet & 0xF8) == 0xF0 ? 4 : 0;
            value = (octet & 0x80) == 0x00 ? octet & 0x7F :
                    (octet & 0xE0) == 0xC0 ? octet & 0x1F :
                    (octet & 0xF0) == 0xE0 ? octet & 0x0F :
                    (octet & 0xF8) == 0xF0 ? octet & 0x07 : 0;
            for (idx = 1; idx < width; idx ++) {
                octet = OCTET_AT(string, idx);
                value = (value << 6) + (octet & 0x3F);
            }
            string.pointer += width;

            if (!PUT(emitter, '\\')) return 0;

            switch (value)
            {
                case 0x00:
                    if (!PUT(emitter, '0')) return 0;
                    break;

                case 0x07:
                    if (!PUT(emitter, 'a')) return 0;
                    break;

                case 0x08:
                    if (!PUT(emitter, 'b')) return 0;
                    break;

                case 0x09:
                    if (!PUT(emitter, 't')) return 0;
                    break;

                case 0x0A:
                    if (!PUT(emitter, 'n')) return 0;
                    break;

                case 0x0B:
                    if (!PUT(emitter, 'v')) return 0;
                    break;

                case 0x0C:
                    if (!PUT(emitter, 'f')) return 0;
                    break;

                case 0x0D:
                    if (!PUT(emitter, 'r')) return 0;
                    break;

                case 0x1B:
                    if (!PUT(emitter, 'e')) return 0;
                    break;

                case 0x22:
                    if (!PUT(emitter, '\"')) return 0;
                    break;

                case 0x5C:
                    if (!PUT(emitter, '\\')) return 0;
                    break;

                case 0x85:
                    if (!PUT(emitter, 'N')) return 0;
                    break;

                case 0xA0:
                    if (!PUT(emitter, '_')) return 0;
                    break;

                case 0x2028:
                    if (!PUT(emitter, 'L')) return 0;
                    break;

                case 0x2029:
                    if (!PUT(emitter, 'P')) return 0;
                    break;

                default:
                    if (value <= 0xFF) {
                        if (!PUT(emitter, 'x')) return 0;
                        width = 2;
                    }
                    else if (value <= 0xFFFF) {
                        if (!PUT(emitter, 'u')) return 0;
                        width = 4;
                    }
                    else {
                        if (!PUT(emitter, 'U')) return 0;
                        width = 8;
                    }
                    for (idx = (width-1)*4; idx >= 0; idx -= 4) {
                        int digit = (value >> idx) & 0x0F;
                        if (!PUT(emitter, digit + (digit < 10 ? '0' : 'A'-10)))
                            return 0;
                    }
            }
            spaces = 0;
        }
        else if (IS_SPACE(string))
        {
            if (allow_breaks && !spaces
                    && emitter->column > emitter->best_width
                    && string.pointer != 0
                    && string.pointer != string.capacity - 1) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
                if (IS_SPACE_AT(string, 1)) {
                    if (!PUT(emitter, '\\')) return 0;
                }
                MOVE(string);
            }
            else {
                if (!WRITE(emitter, string)) return 0;
            }
            spaces = 1;
        }
        else
        {
            if (!WRITE(emitter, string)) return 0;
            spaces = 0;
        }
    }

    if (!yaml_emitter_write_indicator(emitter, "\"", 0, 0, 0))
        return 0;

    emitter->is_whitespace = 0;
    emitter->is_indention = 0;

    return 1;
}

static int
yaml_emitter_determine_chomping(yaml_emitter_t *emitter,
        yaml_string_t string)
{
    string.pointer = string.capacity;
    if (!string.pointer)
        return -1;
    do {
        string.pointer --;
    } while ((OCTET(string) & 0xC0) == 0x80);
    if (!IS_BREAK(string))
        return -1;
    if (!string.pointer)
        return 0;
    do {
        string.pointer --;
    } while ((OCTET(string) & 0xC0) == 0x80);
    if (!IS_BREAK(string))
        return 0;
    return +1;
    
}

static int
yaml_emitter_write_literal_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length)
{
    yaml_string_t string = STRING(value, length);
    int chomp = yaml_emitter_determine_chomping(emitter, string);
    int breaks = 0;

    if (!yaml_emitter_write_indicator(emitter,
                chomp == -1 ? "|-" : chomp == +1 ? "|+" : "|", 1, 0, 0))
        return 0;
    if (!yaml_emitter_write_indent(emitter))
        return 0;

    while (string.pointer < string.capacity)
    {
        if (IS_BREAK(string))
        {
            if (!WRITE_BREAK(emitter, string)) return 0;
            emitter->is_indention = 1;
            breaks = 1;
        }
        else
        {
            if (breaks) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
            }
            if (!WRITE(emitter, string)) return 0;
            emitter->is_indention = 0;
            breaks = 0;
        }
    }

    return 1;
}

static int
yaml_emitter_write_folded_scalar(yaml_emitter_t *emitter,
        yaml_char_t *value, size_t length)
{
    yaml_string_t string = STRING(value, length);
    int chomp = yaml_emitter_determine_chomping(emitter, string);
    int breaks = 1;
    int leading_spaces = 0;

    if (!yaml_emitter_write_indicator(emitter,
                chomp == -1 ? ">-" : chomp == +1 ? ">+" : ">", 1, 0, 0))
        return 0;
    if (!yaml_emitter_write_indent(emitter))
        return 0;

    while (string.pointer < string.capacity)
    {
        if (IS_BREAK(string))
        {
            if (!breaks && !leading_spaces && CHECK(string, '\n')) {
                int k = 0;
                while (IS_BREAK_AT(string, k)) {
                    k += WIDTH_AT(string, k);
                }
                if (!IS_BLANK_AT(string, k)) {
                    if (!PUT_BREAK(emitter)) return 0;
                }
            }
            if (!WRITE_BREAK(emitter, string)) return 0;
            emitter->is_indention = 1;
            breaks = 1;
        }
        else
        {
            if (breaks) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
                leading_spaces = IS_BLANK(string);
            }
            if (!breaks && IS_SPACE(string) && !IS_SPACE_AT(string, 1)
                    && emitter->column > emitter->best_width) {
                if (!yaml_emitter_write_indent(emitter)) return 0;
                MOVE(string);
            }
            else {
                if (!WRITE(emitter, string)) return 0;
            }
            emitter->is_indention = 0;
            breaks = 0;
        }
    }

    return 1;
}

