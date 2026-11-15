from pathlib import Path
import shutil

from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


SOURCE = Path(r"C:\Users\Lenovo\Downloads\Assignment 2 plan.docx")
GANTT_OUT = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Timeline_Gantt.docx")
WORKFLOW_OUT = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Timeline_Workflow.docx")


def set_run_font(run, size=12, bold=False, italic=False, color="000000"):
    run.font.name = "Arial"
    rpr = run._element.get_or_add_rPr()
    rpr.rFonts.set(qn("w:ascii"), "Arial")
    rpr.rFonts.set(qn("w:hAnsi"), "Arial")
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    run.font.color.rgb = RGBColor.from_string(color)


def set_cell_shading(cell, fill):
    tcpr = cell._tc.get_or_add_tcPr()
    shd = tcpr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tcpr.append(shd)
    shd.set(qn("w:fill"), fill)
    shd.set(qn("w:val"), "clear")


def set_cell_margins(cell, top=80, left=65, bottom=80, right=65):
    tcpr = cell._tc.get_or_add_tcPr()
    tc_mar = tcpr.find(qn("w:tcMar"))
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tcpr.append(tc_mar)
    for edge, value in (("top", top), ("left", left), ("bottom", bottom), ("right", right)):
        element = tc_mar.find(qn(f"w:{edge}"))
        if element is None:
            element = OxmlElement(f"w:{edge}")
            tc_mar.append(element)
        element.set(qn("w:w"), str(value))
        element.set(qn("w:type"), "dxa")


def set_cell_borders(cell, color="000000", size="6"):
    tcpr = cell._tc.get_or_add_tcPr()
    borders = tcpr.find(qn("w:tcBorders"))
    if borders is None:
        borders = OxmlElement("w:tcBorders")
        tcpr.append(borders)
    for edge in ("top", "left", "bottom", "right"):
        node = borders.find(qn(f"w:{edge}"))
        if node is None:
            node = OxmlElement(f"w:{edge}")
            borders.append(node)
        node.set(qn("w:val"), "single")
        node.set(qn("w:sz"), size)
        node.set(qn("w:space"), "0")
        node.set(qn("w:color"), color)


def set_repeat_header(row):
    trpr = row._tr.get_or_add_trPr()
    header = OxmlElement("w:tblHeader")
    header.set(qn("w:val"), "true")
    trpr.append(header)


def fill_cell(cell, text, size=9.0, bold=False, align=WD_ALIGN_PARAGRAPH.LEFT, color="000000"):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = align
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing = 1.0
    run = p.add_run(text)
    set_run_font(run, size=size, bold=bold, color=color)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    set_cell_margins(cell)
    set_cell_borders(cell)


def prepare_document(output, title, intro_text):
    shutil.copy2(SOURCE, output)
    doc = Document(output)
    body = doc._element.body
    sect_pr = body.sectPr
    for child in list(body):
        if child is not sect_pr:
            body.remove(child)
    section = doc.sections[0]
    section.page_width = Inches(8.27)
    section.page_height = Inches(11.69)
    section.left_margin = Inches(0.72)
    section.right_margin = Inches(0.72)
    section.top_margin = Inches(0.72)
    section.bottom_margin = Inches(0.72)

    heading = doc.add_paragraph()
    heading.paragraph_format.space_before = Pt(8)
    heading.paragraph_format.space_after = Pt(3)
    heading.paragraph_format.keep_with_next = True
    set_run_font(heading.add_run(title), size=18, bold=True)

    intro = doc.add_paragraph()
    intro.paragraph_format.space_after = Pt(4)
    intro.paragraph_format.line_spacing = 1.0
    set_run_font(intro.add_run(intro_text), size=11)
    return doc


def add_caption(doc, text):
    caption = doc.add_paragraph()
    caption.paragraph_format.space_before = Pt(5)
    caption.paragraph_format.space_after = Pt(3)
    caption.paragraph_format.keep_with_next = True
    set_run_font(caption.add_run(text), size=11, bold=True)


def add_note(doc, text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing = 1.0
    set_run_font(p.add_run(text), size=10)


def build_gantt():
    doc = prepare_document(
        GANTT_OUT,
        "Project Timeline",
        "The project runs from 23 October to 11 November 2026 and is completed before the 12 November deadline. "
        "Task duration is proportional to estimated difficulty, while dependent tasks overlap only where hand-built ASTs permit parallel work.",
    )
    add_caption(doc, "Table 5. Dated Gantt timeline")

    periods = ["23-27\nOct", "28 Oct-\n1 Nov", "2-6\nNov", "7-11\nNov"]
    headers = ["ID", "Task", "Owner", "Level"] + periods
    rows = [
        ("T1", "Language specification and grammar", "Joint", "Medium", [1]),
        ("T2", "Shared interfaces", "Joint", "Low", [1]),
        ("T3", "Lexer", "Bruce", "Medium", [1]),
        ("T4", "Parser and AST", "Bruce", "High", [2]),
        ("T5", "Syntax errors and recovery", "Bruce", "Medium", [3]),
        ("T6", "Semantic analysis", "Phong", "Medium", [3]),
        ("T7", "Interpreter core", "Phong", "High", [1, 2]),
        ("T8", "Runtime and language features", "Phong", "High", [3]),
        ("T9", "Test suite and automated runner", "Joint", "Medium", [3, 4]),
        ("T10", "Integration, guide and final report", "Joint", "Medium", [4]),
    ]
    widths = [0.38, 2.25, 0.74, 0.68, 0.64, 0.72, 0.64, 0.64]
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    for i, (cell, label) in enumerate(zip(table.rows[0].cells, headers)):
        cell.width = Inches(widths[i])
        fill_cell(cell, label, size=8.6, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER)
        set_cell_shading(cell, "F2F2F2")
    set_repeat_header(table.rows[0])

    for task_id, task, owner, level, active in rows:
        cells = table.add_row().cells
        fixed = [task_id, task, owner, level]
        for i, value in enumerate(fixed):
            cells[i].width = Inches(widths[i])
            fill_cell(cells[i], value, size=8.6, align=WD_ALIGN_PARAGRAPH.CENTER if i != 1 else WD_ALIGN_PARAGRAPH.LEFT)
        for period in range(1, 5):
            i = period + 3
            cells[i].width = Inches(widths[i])
            fill_cell(cells[i], "", size=8.6, align=WD_ALIGN_PARAGRAPH.CENTER)
            if period in active:
                set_cell_shading(cells[i], "7F7F7F")

    legend = doc.add_paragraph()
    legend.paragraph_format.space_before = Pt(3)
    legend.paragraph_format.space_after = Pt(2)
    set_run_font(legend.add_run("Grey cells indicate scheduled work. Joint tasks are shared by Bruce and Phong."), size=9.5, italic=True)

    add_note(
        doc,
        "Detailed allocation: T1 (23-24 Oct), T2 (25 Oct), T3 (26-27 Oct), T7 (26-30 Oct), "
        "T4 (28 Oct-1 Nov), T5 and T6 (2-3 Nov), T8 (2-6 Nov), T9 (4-8 Nov), and T10 (7-11 Nov). "
        "The final internal review is completed on 11 November, leaving the 12 November deadline free of core development.",
    )
    doc.core_properties.title = "Flux Dated Gantt Timeline"
    doc.save(GANTT_OUT)


def build_workflow():
    doc = prepare_document(
        WORKFLOW_OUT,
        "Project Workflow",
        "The workflow converts the task dependencies into six dated stages. Each stage has a completion gate so that later work begins only when the required interfaces or behaviours are stable.",
    )
    add_caption(doc, "Table 5. Dated implementation workflow")

    headers = ["Stage and dates", "Bruce", "Phong", "Completion gate"]
    rows = [
        (
            "1  Specification\n23-24 Oct",
            "T1 with Phong",
            "T1 with Bruce",
            "Flux constructs, LL(1) grammar and semantic rules agreed.",
        ),
        (
            "2  Interface lock\n25 Oct",
            "T2 with Phong",
            "T2 with Bruce",
            "Token.h, AST.h and Value.h agreed before implementation branches diverge.",
        ),
        (
            "3  Parallel core build\n26 Oct-1 Nov",
            "T3 Lexer; T4 Parser and AST",
            "T7 Interpreter core using hand-built ASTs",
            "Lexer and parser produce the agreed AST; core expressions and control flow execute.",
        ),
        (
            "4  Analysis and runtime\n2-6 Nov",
            "T5 Syntax error reporting and recovery",
            "T6 Semantic analysis; T8 scope, functions, history, arrays and I/O",
            "Semantic and runtime checks pass component tests; high-difficulty runtime features are integrated.",
        ),
        (
            "5  System verification\n4-8 Nov",
            "T9 tests with Phong",
            "T9 tests with Bruce",
            "Catch2 and golden-file suites pass, including the stock-ledger end-to-end example.",
        ),
        (
            "6  Integration and release\n7-11 Nov",
            "T10 review, guide and report",
            "T10 CI, release checks and report review",
            "All tests pass; user guide and report are complete; final internal sign-off occurs on 11 Nov.",
        ),
    ]

    widths = [1.28, 1.55, 1.55, 2.45]
    table = doc.add_table(rows=1, cols=4)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False
    for i, (cell, label) in enumerate(zip(table.rows[0].cells, headers)):
        cell.width = Inches(widths[i])
        fill_cell(cell, label, size=9.4, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER)
        set_cell_shading(cell, "F2F2F2")
    set_repeat_header(table.rows[0])

    for row_no, row_data in enumerate(rows, start=1):
        cells = table.add_row().cells
        for i, value in enumerate(row_data):
            cells[i].width = Inches(widths[i])
            fill_cell(cells[i], value, size=9.15, bold=(i == 0), align=WD_ALIGN_PARAGRAPH.LEFT)
        if row_no % 2 == 0:
            for cell in cells:
                set_cell_shading(cell, "F7F7F7")

    flow = doc.add_paragraph()
    flow.alignment = WD_ALIGN_PARAGRAPH.CENTER
    flow.paragraph_format.space_before = Pt(6)
    flow.paragraph_format.space_after = Pt(4)
    flow.paragraph_format.line_spacing = 1.0
    set_run_font(
        flow.add_run("T1 -> T2 -> (T3 -> T4 -> T5 -> T6) + (T7 -> T8) -> T9 -> T10"),
        size=10.5,
        bold=True,
    )

    add_note(
        doc,
        "The high-difficulty tasks T4, T7 and T8 receive five-day allocations. T4 and T7 run in parallel after the shared interfaces are fixed, and T6 can begin against hand-built ASTs before the parser is fully merged. "
        "If progress falls behind, the collection loop and input feature are removed first. Core implementation and internal sign-off finish by 11 November, before the 12 November deadline.",
    )
    doc.core_properties.title = "Flux Dated Project Workflow"
    doc.save(WORKFLOW_OUT)


build_gantt()
build_workflow()
print(GANTT_OUT)
print(WORKFLOW_OUT)
