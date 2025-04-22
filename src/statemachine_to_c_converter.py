#!/usr/bin/env python3

# import sys
# import re
# import os
#
# def parse_dot_edge(line):
#     # More forgiving pattern: grab source, destination, and full label content
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     # Split label into parts by '|'
#     parts = [part.strip() for part in label.split('|')]
#
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     # Split actions into lines with semicolons
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#     formatted_actions = '\n  '.join(action_lines)
#
#     c_code = f'// edge "{src}" -> "{dst}", event {event}\n'
#     c_code += f'if ({condition}) {{\n  {formatted_actions}\n}}'
#
#     return c_code
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     output_path = os.path.splitext(input_path)[0] + '.c'
#     converted_lines = []
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             c_code = parse_dot_edge(line)
#             if c_code:
#                 converted_lines.append(c_code)
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️ Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not converted_lines:
#         print("❌ No valid edges found to convert.")
#     else:
#         with open(output_path, 'w') as outfile:
#             outfile.write('\n\n'.join(converted_lines))
#         print(f"✅ Conversion complete! Output written to '{output_path}'.")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file edges into C-style code blocks.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     # Match edge lines with quoted nodes and label
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     # Split label into event | condition | actions
#     parts = [part.strip() for part in label.split('|')]
#
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#     return {
#         "src": src,
#         "dst": dst,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     output_path = os.path.splitext(input_path)[0] + '.c'
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     # Sort states for consistency
#     sorted_states = sorted(all_states)
#
#     with open(output_path, 'w') as outfile:
#         # Write the enum
#         outfile.write("typedef enum {\n")
#         for state in sorted_states:
#             outfile.write(f"    {state},\n")
#         outfile.write("} States;\n\n")
#
#         # Write the switch block
#         outfile.write("switch (current_state) {\n")
#         for state in sorted_states:
#             outfile.write(f"  case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 outfile.write(f"    // edge \"{trans['src']}\" -> \"{trans['dst']}\", event {trans['event']}\n")
#                 outfile.write(f"    if ({trans['condition']}) {{\n")
#                 outfile.write(f"      current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"      {act}\n")
#                 outfile.write("    }\n")
#             outfile.write("    break;\n\n")
#         outfile.write("}\n")
#
#     print(f"✅ Conversion complete! Output written to '{output_path}'.")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C-style enum and switch-case transitions.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states):
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("extern State current_state;\n")
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_name, transitions, states):
#     with open(c_path, 'w') as outfile:
#         outfile.write(f'#include "{header_name}"\n\n')
#         outfile.write("State current_state;\n\n")
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     c_path = base_name + '.c'
#     h_path = base_name + '.h'
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states)
#     generate_c_file(c_path, h_path, transitions, all_states)
#
#     print(f"✅ Generated '{c_path}' and '{h_path}'.")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("extern State current_state;\n\n")
#
#         # Global variables
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n\n")
#
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         outfile.write(f'#include "{header_rel_path}"\n\n')
#
#         # Global state and variables
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n\n")
#
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states)
#     generate_c_file(c_path, header_rel_path, transitions, all_states)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("extern State current_state;\n\n")
#
#         # Global variables
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n\n")
#
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         outfile.write(f'#include "{header_rel_path}"\n\n')
#
#         # Global state and variables
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n\n")
#
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states)
#     generate_c_file(c_path, header_rel_path, transitions, all_states)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("extern State current_state;\n\n")
#
#         # Global variables
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n\n")
#
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         # Include standard and custom headers
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n\n')
#
#         # Global state and variables
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n\n")
#
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states)
#     generate_c_file(c_path, header_rel_path, transitions, all_states)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("extern State current_state;\n\n")
#
#         # Global variables
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n\n")
#
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         # Include standard and custom headers
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n')
#         outfile.write('#include "../include/statemachine_actions.h"\n\n')
#
#         # Global state and variables
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n\n")
#
#         # Static variable
#         outfile.write("static uint8_t finished_here;\n\n")
#
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states)
#     generate_c_file(c_path, header_rel_path, transitions, all_states)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n")
#         header.write("#include <stdint.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("extern State current_state;\n\n")
#
#         # Global variables
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n")
#         header.write("extern uint8_t finished_here;\n")
#         header.write("extern uint8_t stepped_into_here;\n\n")
#
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         # Include standard and custom headers
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n')
#         outfile.write('#include "../include/statemachine_actions.h"\n\n')
#
#         # Global state and variables
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n")
#         outfile.write("uint8_t finished_here;\n")
#         outfile.write("uint8_t stepped_into_here;\n\n")
#
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states)
#     generate_c_file(c_path, header_rel_path, transitions, all_states)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def to_enum_name(name):
#     return name.upper().replace(" ", "_")
#
# def generate_header_file(header_path, states, events):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n")
#         header.write("#include <stdint.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("typedef enum {\n")
#         for event in sorted(events):
#             header.write(f"    {to_enum_name(event)},\n")
#         header.write("} Event;\n\n")
#
#         header.write("extern State current_state;\n")
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n")
#         header.write("extern uint8_t finished_here;\n")
#         header.write("extern uint8_t stepped_into_here;\n\n")
#
#         header.write("void update_state(void);\n\n")
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n')
#         outfile.write('#include "../include/statemachine_actions.h"\n\n')
#
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n")
#         outfile.write("uint8_t finished_here;\n")
#         outfile.write("uint8_t stepped_into_here;\n\n")
#
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             state_transitions = transitions.get(state, [])
#             for trans in state_transitions:
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 event_macro = to_enum_name(trans["event"])
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if ({event_macro} && {trans['condition']}) {{\n")
#                 outfile.write(f"        current_state={trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#     all_events = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#                 all_events.add(parsed['event'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️ Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states, all_events)
#     generate_c_file(c_path, header_rel_path, transitions, all_states)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "event_enum": re.sub(r'\W+', '_', event).upper(),
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states, events):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n")
#         header.write("#include <stdint.h>\n\n")
#
#         # Enum for states
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("}} State;\n\n")
#
#         # Enum for events
#         header.write("typedef enum {\n")
#         for event in sorted(events):
#             header.write(f"    {event},\n")
#         header.write("}} Event;\n\n")
#
#         # Global vars
#         header.write("extern State current_state;\n")
#         header.write("extern Event event;\n\n")
#
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n")
#         header.write("extern uint8_t finished_here;\n")
#         header.write("extern uint8_t stepped_into_here;\n\n")
#
#         header.write("void update_state(void);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states):
#     with open(c_path, 'w') as outfile:
#         # Includes
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n')
#         outfile.write('#include "../include/statemachine_actions.h"\n\n')
#
#         # Global variables
#         outfile.write("State current_state;\n")
#         outfile.write("Event event;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n")
#         outfile.write("uint8_t finished_here;\n")
#         outfile.write("uint8_t stepped_into_here;\n\n")
#
#         # update_state function
#         outfile.write("void update_state(void) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             for trans in transitions[state]:
#                 comment = f'{trans["src"]} -> {trans["dst"]} ({trans["label"]})'
#                 outfile.write(f"      // {comment}\n")
#                 condition = f"(event == {trans['event_enum']} && {trans['condition']})"
#                 outfile.write(f"      if {condition} {{\n")
#                 outfile.write(f"        current_state = {trans['dst']};\n")
#                 for action in trans['actions']:
#                     outfile.write(f"        {action}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"❌ Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     states = set()
#     events = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed["src"]].append(parsed)
#                 states.update([parsed["src"], parsed["dst"]])
#                 events.add(parsed["event_enum"])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️ Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found.")
#         return
#
#     generate_header_file(h_path, states, events)
#     generate_c_file(c_path, header_rel_path, transitions, states)
#     print(f"✅ Generated:\n- C file: {c_path}\n- Header: {h_path}")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts a DOT state machine into C state machine logic with headers.")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print_usage()
#         sys.exit(1)
#     convert_dot_to_c(sys.argv[1])

# import sys
# import re
# import os
# from collections import defaultdict
#
# def sanitize_enum_name(name):
#     return re.sub(r'[^A-Za-z0-9_]', '_', name.strip().upper())
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "event_enum": sanitize_enum_name(event),
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states, events):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n")
#         header.write("#include <stdint.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("} State;\n\n")
#
#         header.write("typedef enum {\n")
#         for event in sorted(events):
#             header.write(f"    {event},\n")
#         header.write("} Event;\n\n")
#
#         header.write("extern State current_state;\n")
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n")
#         header.write("extern uint8_t finished_here;\n")
#         header.write("extern uint8_t stepped_into_here;\n\n")
#
#         header.write("void update_state(Event event);\n\n")
#
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states, events):
#     with open(c_path, 'w') as outfile:
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n')
#         outfile.write('#include "../include/statemachine_actions.h"\n\n')
#
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n")
#         outfile.write("uint8_t finished_here;\n")
#         outfile.write("uint8_t stepped_into_here;\n\n")
#
#         outfile.write("void update_state(Event event) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             for trans in transitions.get(state, []):
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} (event = {trans["event"]} | {trans["condition"]} | {" ".join(trans["actions"])})'
#                 outfile.write(f"      // {full_comment}\n")
#                 outfile.write(f"      if (event == {trans['event_enum']} && {trans['condition']}) {{\n")
#                 outfile.write(f"        current_state = {trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#     all_events = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#                 all_events.add(parsed['event_enum'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states, all_events)
#     generate_c_file(c_path, header_rel_path, transitions, all_states, all_events)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])
#

# import sys
# import re
# import os
# from collections import defaultdict
#
# def sanitize_enum_name(name):
#     return re.sub(r'[^A-Za-z0-9_]', '_', name.strip().upper())
#
# def parse_dot_edge(line):
#     match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
#     if not match:
#         return None
#
#     src, dst, label = match.groups()
#     parts = [part.strip() for part in label.split('|')]
#     event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
#     condition = parts[1] if len(parts) > 1 else "true"
#     actions = parts[2] if len(parts) > 2 else ""
#
#     action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]
#
#     return {
#         "src": src,
#         "dst": dst,
#         "label": label,
#         "event": event,
#         "event_enum": sanitize_enum_name(event),
#         "condition": condition,
#         "actions": action_lines
#     }
#
# def generate_header_file(header_path, states, events):
#     os.makedirs(os.path.dirname(header_path), exist_ok=True)
#     guard = os.path.basename(header_path).replace('.', '_').upper()
#
#     with open(header_path, 'w') as header:
#         header.write(f"#ifndef {guard}\n")
#         header.write(f"#define {guard}\n\n")
#         header.write("#include <stdbool.h>\n")
#         header.write("#include <stdint.h>\n\n")
#
#         header.write("typedef enum {\n")
#         for state in sorted(states):
#             header.write(f"    {state},\n")
#         header.write("}} State;\n\n")
#
#         header.write("typedef enum {\n")
#         for event in sorted(events):
#             header.write(f"    {event},\n")
#         header.write("}} Event;\n\n")
#
#         header.write("extern State current_state;\n")
#         header.write("extern bool breakpoint_encountered;\n")
#         header.write("extern bool isr_finished;\n")
#         header.write("extern bool step_into_activated;\n")
#         header.write("extern bool isr_active;\n")
#         header.write("extern bool si_happened;\n")
#         header.write("extern uint8_t finished_here;\n")
#         header.write("extern uint8_t stepped_into_here;\n\n")
#
#         header.write("void update_state(Event event);\n\n")
#         header.write(f"#endif // {guard}\n")
#
# def generate_c_file(c_path, header_rel_path, transitions, states, events):
#     with open(c_path, 'w') as outfile:
#         outfile.write(f'#include "{header_rel_path}"\n')
#         outfile.write('#include "../include/interrupt_controller.h"\n')
#         outfile.write('#include "../include/datastructures.h"\n')
#         outfile.write('#include "../include/interpr.h"\n')
#         outfile.write('#include "../include/statemachine_actions.h"\n\n')
#
#         outfile.write("State current_state;\n")
#         outfile.write("bool breakpoint_encountered;\n")
#         outfile.write("bool isr_finished;\n")
#         outfile.write("bool step_into_activated;\n")
#         outfile.write("bool isr_active;\n")
#         outfile.write("bool si_happened;\n")
#         outfile.write("uint8_t finished_here;\n")
#         outfile.write("uint8_t stepped_into_here;\n\n")
#
#         outfile.write("void update_state(Event event) {\n")
#         outfile.write("  switch (current_state) {\n")
#
#         for state in sorted(states):
#             outfile.write(f"    case {state}:\n")
#             for trans in transitions.get(state, []):
#                 full_comment = f'{trans["src"]} -> {trans["dst"]} (event = {trans["event"]} | {trans["condition"]} | {" ".join(trans["actions"])})'
#                 outfile.write(f"      // {full_comment}\n")
#
#                 condition = trans['condition'].strip()
#                 if not condition or condition.lower() == "true":
#                     outfile.write(f"      if (event == {trans['event_enum']}) {{\n")
#                 else:
#                     outfile.write(f"      if (event == {trans['event_enum']} && {condition}) {{\n")
#
#                 outfile.write(f"        current_state = {trans['dst']};\n")
#                 for act in trans['actions']:
#                     outfile.write(f"        {act}\n")
#                 outfile.write("      }\n")
#             outfile.write("      break;\n\n")
#
#         outfile.write("  }\n")
#         outfile.write("}\n")
#
# def convert_dot_to_c(input_path):
#     if not os.path.exists(input_path):
#         print(f"Error: File '{input_path}' not found.")
#         sys.exit(1)
#
#     base_name = os.path.splitext(os.path.basename(input_path))[0]
#     h_dir = "../include"
#     h_path = os.path.join(h_dir, base_name + '.h')
#     c_path = base_name + '.c'
#     header_rel_path = f"../include/{base_name}.h"
#
#     transitions = defaultdict(list)
#     all_states = set()
#     all_events = set()
#
#     with open(input_path, 'r') as infile:
#         for line in infile:
#             parsed = parse_dot_edge(line)
#             if parsed:
#                 transitions[parsed['src']].append(parsed)
#                 all_states.add(parsed['src'])
#                 all_states.add(parsed['dst'])
#                 all_events.add(parsed['event_enum'])
#             elif '->' in line and 'label=' in line:
#                 print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")
#
#     if not transitions:
#         print("❌ No valid edges found to convert.")
#         return
#
#     generate_header_file(h_path, all_states, all_events)
#     generate_c_file(c_path, header_rel_path, transitions, all_states, all_events)
#
#     print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")
#
# def print_usage():
#     print("Usage: python dot_to_c_converter.py <input_file.dot>")
#     print("Converts DOT file into C code and header defining state machine logic.")
#     print("Example: python dot_to_c_converter.py statemachine.dot")
#
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("\n❌ Incorrect usage.\n")
#         print_usage()
#         sys.exit(1)
#
#     convert_dot_to_c(sys.argv[1])
#

import sys
import re
import os
from collections import defaultdict

def sanitize_enum_name(name):
    return re.sub(r'[^A-Za-z0-9_]', '_', name.strip().upper())

def parse_dot_edge(line):
    match = re.search(r'"([^"]+)"\s*->\s*"([^"]+)"\s*\[label="(.+?)"\]', line.strip())
    if not match:
        return None

    src, dst, label = match.groups()
    parts = [part.strip() for part in label.split('|')]
    event = parts[0] if len(parts) > 0 else "UNKNOWN_EVENT"
    condition = parts[1] if len(parts) > 1 else "true"
    actions = parts[2] if len(parts) > 2 else ""

    action_lines = [act.strip() + ";" for act in actions.split(';') if act.strip()]

    return {
        "src": src,
        "dst": dst,
        "label": label,
        "event": event,
        "event_enum": sanitize_enum_name(event),
        "condition": condition,
        "actions": action_lines
    }

def generate_header_file(header_path, states, events):
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    guard = os.path.basename(header_path).replace('.', '_').upper()

    with open(header_path, 'w') as header:
        header.write(f"#ifndef {guard}\n")
        header.write(f"#define {guard}\n\n")
        header.write("#include <stdbool.h>\n")
        header.write("#include <stdint.h>\n\n")

        header.write("typedef enum {\n")
        for state in sorted(states):
            header.write(f"    {state},\n")
        header.write("} State;\n\n")

        header.write("typedef enum {\n")
        for event in sorted(events):
            header.write(f"    {event},\n")
        header.write("} Event;\n\n")

        header.write("extern State current_state;\n")
        header.write("extern bool breakpoint_encountered;\n")
        header.write("extern bool isr_finished;\n")
        header.write("extern bool step_into_activated;\n")
        header.write("extern bool isr_active;\n")
        header.write("extern bool si_happened;\n")
        header.write("extern uint8_t finished_here;\n")
        header.write("extern uint8_t stepped_into_here;\n\n")

        header.write("void update_state(Event event);\n\n")
        header.write(f"#endif // {guard}\n")

def generate_c_file(c_path, header_rel_path, transitions, states, events):
    with open(c_path, 'w') as outfile:
        outfile.write(f'#include "{header_rel_path}"\n')
        outfile.write('#include "../include/interrupt_controller.h"\n')
        outfile.write('#include "../include/datastructures.h"\n')
        outfile.write('#include "../include/interpr.h"\n')
        outfile.write('#include "../include/statemachine_actions.h"\n\n')

        outfile.write("State current_state;\n")
        outfile.write("bool breakpoint_encountered;\n")
        outfile.write("bool isr_finished;\n")
        outfile.write("bool step_into_activated;\n")
        outfile.write("bool isr_active;\n")
        outfile.write("bool si_happened;\n")
        outfile.write("uint8_t finished_here;\n")
        outfile.write("uint8_t stepped_into_here;\n\n")

        outfile.write("void update_state(Event event) {\n")
        outfile.write("  switch (current_state) {\n")

        for state in sorted(states):
            outfile.write(f"    case {state}:\n")
            for trans in transitions.get(state, []):
                full_comment = f'{trans["src"]} -> {trans["dst"]} (event = {trans["event"]} | {trans["condition"]} | {" ".join(trans["actions"])})'
                outfile.write(f"      // {full_comment}\n")

                condition = trans['condition'].strip()
                if not condition or condition.lower() == "true":
                    outfile.write(f"      if (event == {trans['event_enum']}) {{\n")
                else:
                    outfile.write(f"      if (event == {trans['event_enum']} && {condition}) {{\n")

                outfile.write(f"        current_state = {trans['dst']};\n")
                for act in trans['actions']:
                    outfile.write(f"        {act}\n")
                outfile.write("      }\n")
            outfile.write("      break;\n\n")

        outfile.write("  }\n")
        outfile.write("}\n")

def convert_dot_to_c(input_path):
    if not os.path.exists(input_path):
        print(f"Error: File '{input_path}' not found.")
        sys.exit(1)

    base_name = os.path.splitext(os.path.basename(input_path))[0]
    h_dir = "../include"
    h_path = os.path.join(h_dir, base_name + '.h')
    c_path = base_name + '.c'
    header_rel_path = f"../include/{base_name}.h"

    transitions = defaultdict(list)
    all_states = set()
    all_events = set()

    with open(input_path, 'r') as infile:
        for line in infile:
            parsed = parse_dot_edge(line)
            if parsed:
                transitions[parsed['src']].append(parsed)
                all_states.add(parsed['src'])
                all_states.add(parsed['dst'])
                all_events.add(parsed['event_enum'])
            elif '->' in line and 'label=' in line:
                print(f"⚠️  Warning: Couldn't parse line:\n{line.strip()}")

    if not transitions:
        print("❌ No valid edges found to convert.")
        return

    generate_header_file(h_path, all_states, all_events)
    generate_c_file(c_path, header_rel_path, transitions, all_states, all_events)

    print(f"✅ Generated:\n- C File: '{c_path}'\n- Header: '{h_path}'")

def print_usage():
    print("Usage: python dot_to_c_converter.py <input_file.dot>")
    print("Converts DOT file into C code and header defining state machine logic.")
    print("Example: python dot_to_c_converter.py statemachine.dot")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("\n❌ Incorrect usage.\n")
        print_usage()
        sys.exit(1)

    convert_dot_to_c(sys.argv[1])

