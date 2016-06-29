//  This *may* be a valid warning normally, but the fact is that
//  some Windows messages are negative, and they *still* work in case statements!
//lint -e650  Constant '4294967141' out of range for operator 'case'

//lint -e10    Expecting '}'
//lint -e1066  Symbol declared as "C" conflicts with ...

//lint -esym(1714, CVListView::recalc_dx, CVListView::hide_horiz_scrollbar, CVListView::HitTest)
//lint -esym(1714, CVListView::GetItemState, CVListView::insert_column_header, CVListView::delete_column)
//lint -esym(1714, CTerminal::resize_terminal_pixels, CTerminal::get_terminal_dimens)
//lint -esym(1714, CTerminal::copy_list_to_clipboard, CTerminal::put)

//lint -e525   Negative indentation from line ...
//lint -e539   Did not expect positive indentation from line ...
//lint -e534   Ignoring return value of function
//lint -e716   while(1) ... 

//lint -esym(1784, WinMain)
//lint -e1704  Constructor 'CStatusBar::CStatusBar(const CStatusBar &)' has private access specification
//lint -e1719  assignment operator for class has non-reference parameter
//lint -e1720  assignment operator for class has non-const parameter
//lint -e1722  assignment operator for class does not return a reference to class

//lint -e755   global macro not referenced
//lint -e768   global struct member not referenced

//  new, mostly useless Lint9 warnings
//lint -e730   Boolean argument to function that takes boolean argument
//lint -e801   Use of goto is deprecated
//lint -e845   The left argument to operator '&&' is certain to be 0
//lint -e818   Pointer parameter could be declared as pointing to const
//lint -e830   Location cited in prior message
//lint -e831   Reference cited in prior message
//lint -e834   Operator '-' followed by operator '-' is confusing.  Use parentheses.
//lint -e840   Use of nul character in a string literal (is often appropriate)
//lint -e1776  Converting a string literal to char * is not const safe

//  eventually I should resolve all of these, once I get all the
//  other lint warnings resolved.
//lint -e713  Loss of precision (arg. no. 1) (unsigned int to int)
//lint -e732  Loss of sign (initialization) (int to unsigned long)

