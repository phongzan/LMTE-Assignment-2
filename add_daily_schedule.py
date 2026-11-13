from pathlib import Path

from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


SOURCE = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Assignment_2a_final.docx")
OUTPUT = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Assignment_2_final_with_daily_schedule.docx")


def replace_paragraph(doc, starts_with, text):
    for p in doc.paragraphs:
        if p.text.strip().startswith(starts_with):
            if p.runs:
                p.runs[0].text = text
                for run in p.runs[1:]:
                    run.text = ""
            else:
                p.add_run(text)
            return
    raise ValueError(f"Paragraph not found: {starts_with}")


def set_cell_text(cell, text, bold=False, size=9, align=WD_ALIGN_PARAGRAPH.LEFT):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = align
    p.paragraph_format.space_after = Pt(0)
    run = p.add_run(text)
    run.bold = bold
    run.font.size = Pt(size)
    run.font.name = "Arial"
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), "Arial")
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), "Arial")
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER


def set_repeat_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    marker = OxmlElement("w:tblHeader")
    marker.set(qn("w:val"), "true")
    tr_pr.append(marker)


def set_fill(cell, color):
    tc_pr = cell._tc.get_or_add_tcPr()
    for old in tc_pr.findall(qn("w:shd")):
        tc_pr.remove(old)
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), color)
    tc_pr.append(shd)


doc = Document(SOURCE)

# Update the dated workflow summary and project-timeline narrative.
replace_paragraph(
    doc,
    "The workflow converts the task dependencies",
    "The workflow converts the task dependencies into six dated stages beginning on 28 October: "
    "specification and interface lock (28 Oct), parallel foundations (29-30 Oct), parser and runtime "
    "build (31 Oct-2 Nov), analysis, recovery and component testing (3-4 Nov), integration and system "
    "verification (5-9 Nov), and report completion and release (10-11 Nov). Each stage has a completion "
    "gate, so later work begins only when the required interfaces or behaviours are stable. Table F1 in "
    "Appendix F lists the work of each member and the gate for every stage; Table 4 below shows the schedule."
)
replace_paragraph(
    doc,
    "The project runs from 23 October",
    "The revised project schedule runs from 28 October to 11 November 2026 and finishes before the "
    "12 November deadline. Task duration is proportional to estimated difficulty, while dependent "
    "tasks overlap only where hand-built ASTs permit parallel work."
)

# Revise Appendix F workflow dates and work allocations.
workflow = doc.tables[11]
workflow_rows = [
    ("1  Specification and interface lock\n28 Oct", "T1 and T2 with Phong; confirm Value.h and runtime interfaces", "T1 and T2 with Dongze Yu(Bruce); confirm Token.h and AST.h", "Flux constructs, LL(1) grammar, semantic rules and shared interfaces are agreed."),
    ("2  Parallel foundations\n29-30 Oct", "T7 expression evaluator and value/history storage using hand-built ASTs", "T3 lexer, token positions, NEWLINE insertion and lexer tests", "Lexer emits the agreed tokens; core expressions and history storage execute."),
    ("3  Parser and runtime build\n31 Oct-2 Nov", "T7 control flow and functions; T8 environments, scope, arrays and history", "T4 expression, statement and control-flow parsing; complete AST construction", "Parser produces the agreed AST; core control flow, functions and runtime values execute."),
    ("4  Analysis and component tests\n3-4 Nov", "T6 semantic analysis; T8 runtime completion and runtime unit tests", "T5 syntax recovery and diagnostics; parser and lexer unit tests", "Front-end and back-end component suites pass with stable error formats."),
    ("5  Integration and system verification\n5-9 Nov", "T9 integration and runtime/error tests with Phong", "T9 integration and language-feature tests with Dongze Yu(Bruce)", "The full pipeline passes feature, error, stock-ledger and regression tests in CI."),
    ("6  Report completion and release\n10-11 Nov", "T10 CI, release checks, user guide and report review", "T10 report formatting, rubric check and final review", "All tests pass; report and user guide are complete; final sign-off occurs on 11 Nov."),
]
for idx, values in enumerate(workflow_rows, start=1):
    for col, value in enumerate(values):
        set_cell_text(workflow.cell(idx, col), value, bold=(col == 0), size=8.5)

# Revise Appendix H Gantt period headings and shading to match the new start date.
gantt = doc.tables[12]
for col, label in zip(range(4, 8), ("28-31\nOct", "1-4\nNov", "5-8\nNov", "9-11\nNov")):
    set_cell_text(gantt.cell(0, col), label, bold=True, size=8.5, align=WD_ALIGN_PARAGRAPH.CENTER)

active_periods = {
    "T1": {4},
    "T2": {4},
    "T3": {4},
    "T4": {4, 5},
    "T5": {5},
    "T6": {5},
    "T7": {4, 5},
    "T8": {4, 5},
    "T9": {5, 6, 7},
    "T10": {7},
}
for row in gantt.rows[1:]:
    task_id = row.cells[0].text.strip()
    for col in range(4, 8):
        set_fill(row.cells[col], "8C8C8C" if col in active_periods.get(task_id, set()) else "FFFFFF")

replace_paragraph(
    doc,
    "Detailed allocation: T1",
    "Detailed allocation: T1 and T2 (28 Oct), T3 (29-30 Oct), T7 (29 Oct-1 Nov), T4 (31 Oct-2 Nov), "
    "T5 and T6 (3 Nov), T8 (31 Oct-4 Nov), T9 (4-9 Nov), and T10 (10-11 Nov). The final internal "
    "review is completed on 11 November, leaving the 12 November deadline free of core development."
)

# Append a concrete daily checklist.
doc.add_page_break()
heading = doc.add_paragraph()
heading.paragraph_format.space_after = Pt(5)
run = heading.add_run("Appendix I. Daily Work Schedule")
run.bold = True
run.font.size = Pt(12)
run.font.name = "Arial"
run.font.color.rgb = RGBColor(0, 0, 0)

caption = doc.add_paragraph()
caption.paragraph_format.space_after = Pt(4)
run = caption.add_run("Table I1. Daily implementation checklist from 28 October to 11 November 2026")
run.bold = True
run.font.size = Pt(10)
run.font.name = "Arial"

intro = doc.add_paragraph(
    "Each day ends with committed code, updated tests and a short status update on the shared project board. "
    "A task is complete only when its listed checkpoint is met."
)
intro.paragraph_format.space_after = Pt(6)
for run in intro.runs:
    run.font.size = Pt(9)
    run.font.name = "Arial"

schedule = [
    ("28 Oct", "Agree Token.h, AST.h and front-end acceptance examples; freeze grammar changes needed by the lexer.", "Agree Value.h, environment and history interfaces; prepare interpreter skeleton.", "Freeze T1/T2 interfaces and create implementation branches."),
    ("29 Oct", "Implement identifiers, keywords, literals, comments and one-character tokens in the lexer.", "Implement Flux value representation and evaluation of literals and unary/binary expressions.", "Commit compiling lexer and interpreter foundations."),
    ("30 Oct", "Implement two-character operators, NEWLINE insertion, line/column tracking and lexer unit tests U1-U3.", "Implement variable assignment, constants and per-variable history storage; add history unit tests.", "Review lexer token names and AST/value compatibility."),
    ("31 Oct", "Implement AST nodes and recursive-descent parsing for expressions, precedence and assignment.", "Implement environment chains, local/outer lookup and basic runtime error handling.", "Confirm expression ASTs execute correctly on hand-built and parsed examples."),
    ("1 Nov", "Implement parsing for when/orwhen/otherwise, repeat, until and each.", "Implement selection and loop execution, function calls, give and recursion limits.", "Run control-flow examples P6-P10."),
    ("2 Nov", "Implement functions, arrays, pipelines, history syntax, input/output and complete parser tests U4-U6.", "Implement arrays, element writes, rewind, history(x), x~n, ask and show.", "Merge a complete parse-to-execution path for representative programs."),
    ("3 Nov", "Implement panic-mode syntax recovery and standard lexical/syntax error messages; run E1-E8.", "Implement semantic checks for constants, Boolean conditions, give, duplicate parameters and outer targets; run E9-E14.", "Freeze the common error-message format."),
    ("4 Nov", "Fix front-end defects and add missing lexer/parser regression tests.", "Complete runtime errors, memory handling, constant folding and runtime tests U7 and E15-E25.", "All component tests pass independently."),
    ("5 Nov", "Connect lexer and parser to the integrated executable; fix token/AST mismatches.", "Connect semantic analysis and interpreter to parsed ASTs; fix value/environment mismatches.", "Build ./flux program.flx successfully through the full pipeline."),
    ("6 Nov", "Run and repair tests for literals, variables, constants, arithmetic, logic, selection and loops.", "Repair interpreter behaviour found by P1-P9 and verify history is preserved by assignments.", "Passing tests P1-P9 and their corresponding error cases."),
    ("7 Nov", "Run programs covering functions, pipelines, scope, arrays, history and input/output.", "Repair runtime behaviour found by P10-P19 and verify copy-on-write arrays and outer writes.", "Passing tests P10-P19 and their corresponding error cases."),
    ("8 Nov", "Validate parser and diagnostics for the integration programs P20-P23.", "Validate stock-ledger undo, nested loops, memory behaviour and optimisation in P20-P23.", "All planned passing and failing programs execute with exact expected output."),
    ("9 Nov", "Review test coverage table and add any missing front-end test; review Bruce's changes.", "Run memory/CI checks, add missing runtime test and review Phong's changes.", "CI passes the full Catch2 and golden-file suites; tag v0.9."),
    ("10 Nov", "Update syntax, grammar, testing and front-end allocation sections; check table references.", "Update compiler structure, implementation method and back-end allocation sections; draft user guide.", "Reconcile the report with the implemented system and check every rubric item."),
    ("11 Nov", "Proofread English, verify examples and perform final formatting check.", "Run a clean build and complete release checks; verify repository instructions.", "Export the final PDF, perform joint sign-off and tag v1.0 before the 12 Nov deadline."),
]

widths = [Inches(0.75), Inches(2.1), Inches(2.1), Inches(1.8)]
headers = ["Date", "Phong", "Dongze Yu(Bruce)", "Daily checkpoint"]


def add_schedule_table(rows, stripe_offset=0):
    table = doc.add_table(rows=1, cols=4)
    table.style = gantt.style
    table.autofit = False
    for cell, text, width in zip(table.rows[0].cells, headers, widths):
        cell.width = width
        set_cell_text(cell, text, bold=True, size=8.5, align=WD_ALIGN_PARAGRAPH.CENTER)
        set_fill(cell, "D9E2F3")
    set_repeat_header(table.rows[0])
    for local_index, values in enumerate(rows, start=1):
        cells = table.add_row().cells
        for i, (cell, text, width) in enumerate(zip(cells, values, widths)):
            cell.width = width
            set_cell_text(cell, text, bold=(i == 0), size=8)
            if (local_index + stripe_offset) % 2 == 0:
                set_fill(cell, "F2F2F2")
    return table


# Split the checklist across two balanced pages instead of leaving one row by itself.
add_schedule_table(schedule[:8])
doc.add_page_break()
continued = doc.add_paragraph()
continued.paragraph_format.space_after = Pt(5)
run = continued.add_run("Table I1. Daily implementation checklist continued")
run.bold = True
run.font.size = Pt(10)
run.font.name = "Arial"
add_schedule_table(schedule[8:], stripe_offset=8)

doc.save(OUTPUT)
print(OUTPUT)
