;;; prolac-mode.el --- definitions for Prolac Mode, a kind of CC Mode
;;; for GNU Emacs version 20

;; This file was copied, with modifications, from Emacs Lisp files
;; distributed with GNU Emacs -- particularly, `cc-mode.el', `cc-langs.el',
;; and `font-lock.el'. The original files are (C) The Free Software
;; Foundation. Changes are (C) Eddie Kohler, and may be redistributed under
;; the same terms as the original files. The original GNU copyleft notice
;; follows.

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.


(require 'cc-mode)
(require 'cc-langs)
(require 'font-lock)

;; keyword recognition
(defconst c-Prolac-class-key "\\(class\\|module\\)")
(defconst c-Prolac-method-key "^\\s *\\(exception\\s *\\)?\\(\\sw\\|\\s_\\)*")

(defvar prolac-mode-abbrev-table nil
  "Abbreviation table used in prolac-mode buffers.")
(define-abbrev-table 'prolac-mode-abbrev-table ())

(defvar prolac-mode-map ()
  "Keymap used in prolac-mode buffers.")
(if prolac-mode-map
    ()
  (setq prolac-mode-map (c-make-inherited-keymap))
  ;; change bindings for many characters to remove autoindenting;
  ;; Prolac is so random that autoindenting is a disaster
  (define-key prolac-mode-map ":" 'self-insert-command) 
  (define-key prolac-mode-map "{" 'self-insert-command) 
  (define-key prolac-mode-map "}" 'self-insert-command) 
  (define-key prolac-mode-map "/" 'self-insert-command) 
  (define-key prolac-mode-map "*" 'self-insert-command) 
  (define-key prolac-mode-map "," 'self-insert-command) 
  (define-key prolac-mode-map "\C-m" 'newline) 
  (define-key prolac-mode-map ";" 'self-insert-command))

(defvar prolac-mode-syntax-table nil
  "Syntax table used in prolac-mode buffers.")
(if prolac-mode-syntax-table
    ()
  (setq prolac-mode-syntax-table (make-syntax-table))
  (c-populate-syntax-table prolac-mode-syntax-table)
  ;; `-' is a word character in Prolac
  (modify-syntax-entry ?- "_" prolac-mode-syntax-table))

;;;###autoload
(defun prolac-mode ()
  "Major mode for editing Prolac code.
To submit a problem report, enter `\\[c-submit-bug-report]' from a
prolac-mode buffer.  This automatically sets up a mail buffer with
version information already added.  You just need to add a description
of the problem, including a reproducible test case, and send the
message.

To see what version of CC Mode you are running, enter `\\[c-version]'.

The hook variable `prolac-mode-hook' is run with no args, if that
variable is bound and has a non-nil value.  Also the hook
`c-mode-common-hook' is run first.

Key bindings:
\\{prolac-mode-map}"
  (interactive)
  (c-initialize-cc-mode)
  (kill-all-local-variables)
  (set-syntax-table prolac-mode-syntax-table)
  (setq major-mode 'prolac-mode
	mode-name "Prolac"
	local-abbrev-table prolac-mode-abbrev-table)
  (use-local-map prolac-mode-map)
  (c-common-init)
  (setq comment-start "// "
	comment-end ""
	c-conditional-key nil
	c-method-key c-Prolac-method-key
	c-comment-start-regexp c-C++-comment-start-regexp
	c-class-key c-Prolac-class-key
	c-recognize-knr-p nil
        imenu-case-fold-search nil)
  (c-set-offset 'label '+)
  (run-hooks 'c-mode-common-hook)
  (run-hooks 'prolac-mode-hook)
  (c-update-modeline))

;;; Prolac Font Lock.

(defcustom prolac-font-lock-extra-types '("[A-Z]\\sw+")
  "*List of extra types to fontify in Prolac mode.
Each list item should be a regexp not containing word-delimiters.
For example, a value of (\"[A-Z]\\\\sw+\") means capitalised words are
treated as type names.

The value of this variable is used when Font Lock mode is turned on."
  :type 'font-lock-extra-types-widget
  :group 'font-lock-extra-types)

(defconst prolac-font-lock-keywords-1 nil
  "Subdued level highlighting for Prolac mode.")

(defconst prolac-font-lock-keywords-2 nil
  "Medium level highlighting for Prolac mode.
See also `prolac-font-lock-extra-types'.")

(defconst prolac-font-lock-keywords-3 nil
  "Gaudy level highlighting for Prolac mode.
See also `prolac-font-lock-extra-types'.")

;; Regexps modified from Java and C++'s regexps by Eddie Kohler
;; <eddietwo@lcs.mit.edu>.
(let* ((prolac-keywords
	(eval-when-compile
	  (regexp-opt
	   '("all" "allstatic" "catch" "constructor"
	     "defaultinline" "else" "elseif" "end"
	     "has" "hide" "if" "in" "inline" "let"
	     "noinline" "notusing" "outline" "pathinline"
	     "rename" "self" "show" "super" "then"
	     "using") t)))
       ;;
       ;; These are immediately followed by an object name.
       (prolac-minor-types
	(eval-when-compile
	  (regexp-opt '("bool" "char" "uchar" "short" "ushort" "int"
			"uint" "long" "ulong" "seqint" "void"))))
       ;;
       ;; These are eventually followed by an object name.
       (prolac-major-types
	(eval-when-compile
	  (regexp-opt
	   '("class" "exception" "export" "field" "module" "static"))))
       ;;
       ;; Random types immediately followed by an object name.
       (prolac-other-types
	'(mapconcat 'identity (cons "\\sw+\\.\\sw+" prolac-font-lock-extra-types)
		    "\\|"))
       (prolac-other-depth `(regexp-opt-depth (,@ prolac-other-types)))
       )
 (setq prolac-font-lock-keywords-1
  (list
   ;;
   ;; Fontify class names.
   '("\\<\\(class\\|module\\)\\>[ \t]*\\(\\(\\sw\\|\\s_\\|\\.\\)+\\)?"
     (1 font-lock-type-face) (2 font-lock-function-name-face nil t))
   ;;
   ;; Fontify package names in import directives.
   '("\\<\\(export\\)\\>[ \t]*\\(\\sw+\\)?"
     (1 font-lock-keyword-face) (2 font-lock-constant-face nil t))
   ;;
   ;; Fontify function name definitions, possibly incorporating class names.
   '("^\\s *\\(static\\s *\\)?\\(\\(\\sw\\|\\.\\)+\\).*\\(::=\\)"
     (2 font-lock-function-name-face nil t)
     (4 font-lock-keyword-face))
   ;;
   ;; Fontify error directives.
   '("^#[ \t]*error[ \t]+\\(.+\\)" 1 font-lock-warning-face prepend)
   ;;
   ;; Fontify filenames in #include <...> preprocessor directives as strings.
   '("^#[ \t]*\\(import\\|include\\)[ \t]*\\(<[^>\"\n]*>?\\)"
     2 font-lock-string-face)
   ;;
   ;; Fontify function macro names.
   '("^#[ \t]*define[ \t]+\\(\\sw+\\)(" 1 font-lock-function-name-face)
   ;;
   ;; Fontify symbol names in #elif or #if ... defined preprocessor directives.
   '("^#[ \t]*\\(elif\\|if\\)\\>"
     ("\\<\\(defined\\)\\>[ \t]*(?\\(\\sw+\\)?" nil nil
      (1 font-lock-builtin-face) (2 font-lock-variable-name-face nil t)))
   ;;
   ;; Fontify otherwise as symbol names, and the preprocessor directive names.
   '("^#[ \t]*\\(\\sw+\\)\\>[ \t!]*\\(\\sw+\\)?"
     (1 font-lock-builtin-face) (2 font-lock-variable-name-face nil t))
   ))

 (setq prolac-font-lock-keywords-2
  (append prolac-font-lock-keywords-1
   (list
    ;;
    ;; Fontify all builtin type specifiers.
    (cons (concat "\\<\\(" prolac-minor-types "\\|" prolac-major-types "\\)\\>")
	  'font-lock-type-face)
    ;;
    ;; Fontify all builtin keywords (except below).
    (concat "\\<" prolac-keywords "\\>")
    ;;
    ;; Fontify keywords and types; the first can be followed by a type list.
    (list "\\(:>\\)\\(\\s \\|\\*\\)*\\(\\(\\sw\\|\\.\\)+\\)?"
	  '(1 font-lock-keyword-face) '(3 font-lock-type-face nil t))
    ;;
    ;; Fontify all constants.
    '("\\<\\(false\\|true\\)\\>" . font-lock-constant-face)
    )))

 (setq prolac-font-lock-keywords-3
  (append prolac-font-lock-keywords-2
   ;;
   ;; More complicated regexps for more complete highlighting for types.
   ;; We still have to fontify type specifiers individually, as Prolac is hairy.
   (list
    ;;
    ;; Fontify random types in casts.
    `(eval .
      (list (concat "(\\(" (,@ prolac-other-types) "\\))"
		    "[ \t]*\\(\\sw\\|[\"\(]\\)")
	    ;; Fontify the type name.
	    '(1 font-lock-type-face)))
    )))
 )

(defvar prolac-font-lock-keywords prolac-font-lock-keywords-1
  "Default expressions to highlight in Prolac mode.
See also `prolac-font-lock-extra-types'.")

(let ((prolac-mode-defaults
       '((prolac-font-lock-keywords prolac-font-lock-keywords-1 
	  prolac-font-lock-keywords-2 prolac-font-lock-keywords-3)
	 nil nil ((?_ . "w") (?- . "w")) beginning-of-defun
	 (font-lock-mark-block-function . mark-defun))))
  (setq font-lock-defaults-alist
	(cons (cons 'prolac-mode prolac-mode-defaults)
	      font-lock-defaults-alist)))
