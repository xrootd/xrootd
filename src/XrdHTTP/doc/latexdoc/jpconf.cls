\NeedsTeXFormat{LaTeX2e}[1995/12/01]
\ProvidesClass{jpconf}
              [2007/03/07 v1.1
 LaTeX class for Journal of Physics: Conference Series]
%\RequirePackage{graphicx}
\newcommand\@ptsize{1}
\newif\if@restonecol
\newif\if@letterpaper
\newif\if@titlepage
\newif\ifiopams
\@titlepagefalse
\@letterpaperfalse
\DeclareOption{a4paper}
   {\setlength\paperheight {297mm}%
    \setlength\paperwidth  {210mm}%
\@letterpaperfalse}
\DeclareOption{letterpaper}
   {\setlength\paperheight {279.4mm}%
    \setlength\paperwidth  {215.9mm}%
\@letterpapertrue}
\DeclareOption{landscape}
   {\setlength\@tempdima   {\paperheight}%
    \setlength\paperheight {\paperwidth}%
    \setlength\paperwidth  {\@tempdima}}
\DeclareOption{twoside}{\@twosidetrue  \@mparswitchtrue}
\renewcommand\@ptsize{1}
%\ExecuteOptions{A4paper, twoside}
\ExecuteOptions{A4paper}
\ProcessOptions
\DeclareMathAlphabet{\bi}{OML}{cmm}{b}{it}
\DeclareMathAlphabet{\bcal}{OMS}{cmsy}{b}{n}
\input{jpconf1\@ptsize.clo}
\setlength\lineskip{1\p@}
\setlength\normallineskip{1\p@}
\renewcommand\baselinestretch{}
\setlength\parskip{0\p@ \@plus \p@}
\@lowpenalty   51
\@medpenalty  151
\@highpenalty 301
\setlength\parindent{5mm}
\setcounter{topnumber}{8}
\renewcommand\topfraction{1}
\setcounter{bottomnumber}{3}
\renewcommand\bottomfraction{.99}
\setcounter{totalnumber}{8}
\renewcommand\textfraction{0.01}
\renewcommand\floatpagefraction{.8}
\setcounter{dbltopnumber}{6}
\renewcommand\dbltopfraction{1}
\renewcommand\dblfloatpagefraction{.8}
\renewcommand{\title}{\@ifnextchar[{\@stitle}{\@ftitle}}
\pretolerance=5000
\tolerance=8000
% Headings for all pages apart from first
%
\def\ps@headings{%
     \let\@oddfoot\@empty
     \let\@evenfoot\@empty
     \let\@oddhead\@empty
     \let\@evenhead\@empty
      %\def\@evenhead{\thepage\hfil\itshape\rightmark}%
      %\def\@oddhead{{\itshape\leftmark}\hfil\thepage}%
      %\def\@evenhead{{\itshape Journal of Physics: Conference Series}\hfill}%
      %\def\@oddhead{\hfill {\itshape Journal of Physics: Conference Series}}%%
      \let\@mkboth\markboth
      \let\sectionmark\@gobble
      \let\subsectionmark\@gobble}
%
% Headings for first page
%
\def\ps@myheadings{\let\@oddfoot\@empty\let\@evenfoot\@empty
    \let\@oddhead\@empty\let\@evenhead\@empty
    \let\@mkboth\@gobbletwo
    \let\sectionmark\@gobble
    \let\subsectionmark\@gobble}
%
\def\@stitle[#1]#2{\markboth{#1}{#1}%
    %\pagestyle{empty}%
     \thispagestyle{myheadings}
    \vspace*{25mm}{\exhyphenpenalty=10000\hyphenpenalty=10000 
    %\Large
\fontsize{18bp}{24bp}\selectfont\bf\raggedright\noindent#2\par}}
\def\@ftitle#1{\markboth{#1}{#1}%
    \thispagestyle{myheadings}
%\pagestyle{empty}%
    \vspace*{25mm}{\exhyphenpenalty=10000\hyphenpenalty=10000 
    %\Large\raggedright\noindent\bf#1\par}
\fontsize{18bp}{24bp}\selectfont\bf\noindent\raggedright#1\par}}
%AUTHOR
\renewcommand{\author}{\@ifnextchar[{\@sauthor}{\@fauthor}}
\def\@sauthor[#1]#2{\markright{#1}    % for production only
   \vspace*{1.5pc}%
   \begin{indented}%
   \item[]\normalsize\bf\raggedright#2
   \end{indented}%
   \smallskip}
\def\@fauthor#1{%\markright{#1} for production only
   \vspace*{1.5pc}%
   \begin{indented}%
   \item[]\normalsize\bf\raggedright#1
   \end{indented}%
   \smallskip}
%E-MAIL
\def\eads#1{\vspace*{5pt}\address{E-mail: #1}}
\def\ead#1{\vspace*{5pt}\address{E-mail: \mailto{#1}}}
\def\mailto#1{{\tt #1}}
%ADDRESS
\newcommand{\address}[1]{\begin{indented}
   \item[]\rm\raggedright #1
   \end{indented}}
\newlength{\indentedwidth}
\newdimen\mathindent
\mathindent = 6pc
\indentedwidth=\mathindent
% FOOTNOTES
%\renewcommand\footnoterule{%
%  \kern-3\p@
%  \hrule\@width.4\columnwidth
%  \kern2.6\p@}
%\newcommand\@makefntext[1]{%
%    \parindent 1em%
%    \noindent
%    \hb@xt@1.8em{\hss\@makefnmark}#1}
% Footnotes: symbols selected in same order as address indicators
% unless optional argument of [<num>] use to specify required symbol, 
% 1=\dag, 2=\ddag, etc
% Usage: \footnote{Text of footnote}
%        \footnote[3]{Text of footnote}
%
\def\footnoterule{}%
\setcounter{footnote}{0}
\long\def\@makefntext#1{\parindent 1em\noindent 
 \makebox[1em][l]{\footnotesize\rm$\m@th{\fnsymbol{footnote}}$}%
 \footnotesize\rm #1}
\def\@makefnmark{\normalfnmark}
\def\normalfnmark{\hbox{${\fnsymbol{footnote}}\m@th$}}
\def\altfnmark{\hbox{$^{\rm Note}\ {\fnsymbol{footnote}}\m@th$}}
\def\footNote#1{\let\@makefnmark\altfnmark\footnote{#1}\let\@makefnmark\normalfnmark}
\def\@thefnmark{\fnsymbol{footnote}}
\def\footnote{\protect\pfootnote}
\def\pfootnote{\@ifnextchar[{\@xfootnote}{\stepcounter{\@mpfn}%
       \begingroup\let\protect\noexpand
       \xdef\@thefnmark{\thempfn}\endgroup
     \@footnotemark\@footnotetext}}
\def\@xfootnote[#1]{\setcounter{footnote}{#1}%
   \addtocounter{footnote}{-1}\footnote}

\newcommand\ftnote{\protect\pftnote}
\newcommand\pftnote[1]{\setcounter{footnote}{#1}%
   \addtocounter{footnote}{-1}\footnote}
\newcommand{\fnm}[1]{\setcounter{footnote}{#1}\footnotetext}

\def\@fnsymbol#1{\ifnum\thefootnote=99\hbox{*}\else^{\thefootnote}\fi\relax}
%
% Address marker
%
\newcommand{\ad}[1]{\noindent\hbox{$^{#1}$}\relax}
\newcommand{\adnote}[2]{\noindent\hbox{$^{#1,}$}\setcounter{footnote}{#2}%
   \addtocounter{footnote}{-1}\footnote}
\def\@tnote{}
\newcounter{oldftnote}
\newcommand{\tnote}[1]{*\gdef\@tnote{%
   \setcounter{oldftnote}{\c@footnote}%
   \setcounter{footnote}{99}%
   \footnotetext{#1}%
   \setcounter{footnote}{\c@oldftnote}\addtocounter{footnote}{-1}}}
%==================
% Acknowledgments (no heading if letter)
% Usage \ack for Acknowledgments, \ackn for Acknowledgement
\def\ack{\section*{Acknowledgments}}
\def\ackn{\section*{Acknowledgment}}
%SECTION DEFINITIONS
\setcounter{secnumdepth}{3}
\newcounter {section}
\newcounter {subsection}[section]
\newcounter {subsubsection}[subsection]
\newcounter {paragraph}[subsubsection]
\newcounter {subparagraph}[paragraph]
\renewcommand \thesection {\arabic{section}}
\renewcommand\thesubsection   {\thesection.\arabic{subsection}}
\renewcommand\thesubsubsection{\thesubsection .\arabic{subsubsection}}
\renewcommand\theparagraph    {\thesubsubsection.\arabic{paragraph}}
\renewcommand\thesubparagraph {\theparagraph.\arabic{subparagraph}}
%\nosections
\def\nosections{\vspace{30\p@ plus12\p@ minus12\p@}
    \noindent\ignorespaces}

%\renewcommand{\@startsection}[6]
%{%
%\if@noskipsec \leavevmode \fi
%\par
%  \@tempskipa #4\relax
%%\@tempskipa 0pt\relax
%  \@afterindenttrue
%  \ifdim \@tempskipa <\z@
%    \@tempskipa -\@tempskipa \@afterindentfalse
%  \fi
%  \if@nobreak
%    \everypar{}%
%  \else
%    \addpenalty\@secpenalty\addvspace\@tempskipa
%  \fi
%  \@ifstar
%    {\@ssect{#3}{#4}{#5}{#6}}%
%    {\@dblarg{\@sect{#1}{#2}{#3}{#4}{#5}{#6}}}}
%\renewcommand{\@sect}[8]{%
%  \ifnum #2>\c@secnumdepth
%    \let\@svsec\@empty
%  \else
%   \refstepcounter{#1}%
%    \protected@edef\@svsec{\@seccntformat{#1}\relax}%
%  \fi
%  \@tempskipa #5\relax
%  \ifdim \@tempskipa>\z@
%    \begingroup
%      #6{%
%        \@hangfrom{\hskip #3\relax\@svsec}%
%          \interlinepenalty \@M #8\@@par}%
%    \endgroup
%    \csname #1mark\endcsname{#7}%
%    \addcontentsline{toc}{#1}{%
%      \ifnum #2>\c@secnumdepth \else
%        \protect\numberline{\csname the#1\endcsname}%
%      \fi
%      #7}%
%  \else
%    \def\@svsechd{%
%      #6{\hskip #3\relax
%      \@svsec #8}%
%      \csname #1mark\endcsname{#7}%
%      \addcontentsline{toc}{#1}{%
%        \ifnum #2>\c@secnumdepth \else
%          \protect\numberline{\csname the#1\endcsname}%
%        \fi
%        #7}}%
%  \fi
% \@xsect{#5}}
%\renewcommand{\@xsect}[1]{%
%  \@tempskipa #1\relax
%  \ifdim \@tempskipa>\z@
%    \par \nobreak
%    \vskip \@tempskipa
%    \@afterheading
%  \else
%    \@nobreakfalse
%    \global\@noskipsectrue
%    \everypar{%
%      \if@noskipsec
%        \global\@noskipsecfalse
%       {\setbox\z@\lastbox}%
%        \clubpenalty\@M
%        \begingroup \@svsechd \endgroup
%        \unskip
%        \@tempskipa #1\relax
%        \hskip -\@tempskipa
%      \else
%        \clubpenalty \@clubpenalty
%        \everypar{}%
%      \fi}%
%  \fi
%  \ignorespaces}
%========================================================================
\newcommand\section{\@startsection {section}{1}{\z@}%
                   {-3.25ex\@plus -1ex \@minus -.2ex}%
                   {1sp}%
                   {\reset@font\normalsize\bfseries\raggedright}}
\newcommand\subsection{\@startsection{subsection}{2}{\z@}%
                   {-3.25ex\@plus -1ex \@minus -.2ex}%
                   {1sp}%
                   {\reset@font\normalsize\itshape\raggedright}}
\newcommand\subsubsection{\@startsection{subsubsection}{3}{\z@}%
                                     {-3.25ex\@plus -1ex \@minus -.2ex}%
                                     {-1em \@plus .2em}%
                                     {\reset@font\normalsize\itshape}}
\newcommand\paragraph{\@startsection{paragraph}{4}{\z@}%
                                    {3.25ex \@plus1ex \@minus.2ex}%
                                    {-1em}%
                                    {\reset@font\normalsize\itshape}}
\newcommand\subparagraph{\@startsection{subparagraph}{5}{\parindent}%
                                       {3.25ex \@plus1ex \@minus .2ex}%
                                       {-1em}%
                                      {\reset@font\normalsize\itshape}}
\def\@sect#1#2#3#4#5#6[#7]#8{\ifnum #2>\c@secnumdepth
     \let\@svsec\@empty\else
     \refstepcounter{#1}\edef\@svsec{\csname the#1\endcsname. }\fi
     \@tempskipa #5\relax
      \ifdim \@tempskipa>\z@
        \begingroup #6\relax
          \noindent{\hskip #3\relax\@svsec}{\interlinepenalty \@M #8\par}%
        \endgroup
       \csname #1mark\endcsname{#7}\addcontentsline
         {toc}{#1}{\ifnum #2>\c@secnumdepth \else
                      \protect\numberline{\csname the#1\endcsname}\fi
                    #7}\else
        \def\@svsechd{#6\hskip #3\relax  %% \relax added 2 May 90
                   \@svsec #8\csname #1mark\endcsname
                      {#7}\addcontentsline
                           {toc}{#1}{\ifnum #2>\c@secnumdepth \else
                             \protect\numberline{\csname the#1\endcsname}\fi
                       #7}}\fi
     \@xsect{#5}}
%
\def\@ssect#1#2#3#4#5{\@tempskipa #3\relax
   \ifdim \@tempskipa>\z@
     \begingroup #4\noindent{\hskip #1}{\interlinepenalty \@M #5\par}\endgroup
   \else \def\@svsechd{#4\hskip #1\relax #5}\fi
    \@xsect{#3}}
% LIST DEFINITIONS
\setlength\leftmargini  {2em}
\leftmargin  \leftmargini
\setlength\leftmarginii  {2em}
\setlength\leftmarginiii {1.8em}
\setlength\leftmarginiv  {1.6em}
  \setlength\leftmarginv  {1em}
  \setlength\leftmarginvi {1em}
\setlength\leftmargin{\leftmargini}
\setlength  \labelsep  {.5em}
\setlength  \labelwidth{\leftmargini}
\addtolength\labelwidth{-\labelsep}
\@beginparpenalty -\@lowpenalty
\@endparpenalty   -\@lowpenalty
\@itempenalty     -\@lowpenalty
\renewcommand\theenumi{\roman{enumi}}
\renewcommand\theenumii{\alph{enumii}}
\renewcommand\theenumiii{\arabic{enumiii}}
\renewcommand\theenumiv{\Alph{enumiv}}
\newcommand\labelenumi{(\theenumi)}
\newcommand\labelenumii{(\theenumii)}
\newcommand\labelenumiii{\theenumiii.}
\newcommand\labelenumiv{(\theenumiv)}
\renewcommand\p@enumii{(\theenumi)}
\renewcommand\p@enumiii{(\theenumi.\theenumii)}
\renewcommand\p@enumiv{(\theenumi.\theenumii.\theenumiii)}
\newcommand\labelitemi{$\m@th\bullet$}
\newcommand\labelitemii{\normalfont\bfseries --}
\newcommand\labelitemiii{$\m@th\ast$}
\newcommand\labelitemiv{$\m@th\cdot$}
\renewcommand \theequation {\@arabic\c@equation}

%%%%%%%%%%%%% Figures
\newcounter{figure}
\renewcommand\thefigure{\@arabic\c@figure}
\def\fps@figure{tbp}
\def\ftype@figure{1}
\def\ext@figure{lof}
\def\fnum@figure{\figurename~\thefigure}
\newenvironment{figure}{\footnotesize\rm\@float{figure}}%
    {\end@float\normalsize\rm}
\newenvironment{figure*}{\footnotesize\rm\@dblfloat{figure}}{\end@dblfloat}
\newcounter{table}
\renewcommand\thetable{\@arabic\c@table}
\def\fps@table{tbp}
\def\ftype@table{2}
\def\ext@table{lot}
\def\fnum@table{\tablename~\thetable}
\newenvironment{table}{\footnotesize\rm\@float{table}}%
   {\end@float\normalsize\rm}
\newenvironment{table*}{\footnotesize\rm\@dblfloat{table}}%
   {\end@dblfloat\normalsize\rm}
\newlength\abovecaptionskip
\newlength\belowcaptionskip
\setlength\abovecaptionskip{10\p@}
\setlength\belowcaptionskip{0\p@}
%Table Environments
%\newenvironment{tableref}[3][\textwidth]{%
%\begin{center}%
%\begin{table}%
%\captionsetup[table]{width=#1}
%\centering\caption{\label{#2}#3}}{\end{table}\end{center}}
%%%%%%%%%%%%%%%%%
%\newcounter{figure}
%\renewcommand \thefigure {\@arabic\c@figure}
%\def\fps@figure{tbp}
%\def\ftype@figure{1}
%\def\ext@figure{lof}
%\def\fnum@figure{\figurename~\thefigure}
%ENVIRONMENT: figure
%\newenvironment{figure}
%               {\@float{figure}}
%               {\end@float}
%ENVIRONMENT: figure*
%\newenvironment{figure*}
%               {\@dblfloat{figure}}
%               {\end@dblfloat}
%ENVIRONMENT: table
%\newcounter{table}
%\renewcommand\thetable{\@arabic\c@table}
%\def\fps@table{tbp}
%\def\ftype@table{2}
%\def\ext@table{lot}
%\def\fnum@table{\tablename~\thetable}
%\newenvironment{table}
%               {\@float{table}}
%               {\end@float}
%ENVIRONMENT: table*
%\newenvironment{table*}
%               {\@dblfloat{table}}
%               {\end@dblfloat}
%\newlength\abovecaptionskip
%\newlength\belowcaptionskip
%\setlength\abovecaptionskip{10\p@}
%\setlength\belowcaptionskip{0\p@}
% CAPTIONS
% Added redefinition of \@caption so captions are not written to 
% aux file therefore less need to \protect fragile commands
%
\long\def\@caption#1[#2]#3{\par\begingroup
    \@parboxrestore
    \normalsize
    \@makecaption{\csname fnum@#1\endcsname}{\ignorespaces #3}\par
  \endgroup}
\long\def\@makecaption#1#2{%
  \vskip\abovecaptionskip
  \sbox\@tempboxa{{\bf #1.} #2}%
 \ifdim \wd\@tempboxa >\hsize
   {\bf #1.} #2\par
 \else
    \global \@minipagefalse
    \hb@xt@\hsize{\hfil\box\@tempboxa\hfil}%
  \fi
  \vskip\belowcaptionskip}
\DeclareOldFontCommand{\rm}{\normalfont\rmfamily}{\mathrm}
\DeclareOldFontCommand{\sf}{\normalfont\sffamily}{\mathsf}
\DeclareOldFontCommand{\tt}{\normalfont\ttfamily}{\mathtt}
\DeclareOldFontCommand{\bf}{\normalfont\bfseries}{\mathbf}
\DeclareOldFontCommand{\it}{\normalfont\itshape}{\mathit}
\DeclareOldFontCommand{\sl}{\normalfont\slshape}{\@nomath\sl}
\DeclareOldFontCommand{\sc}{\normalfont\scshape}{\@nomath\sc}
\DeclareRobustCommand*\cal{\@fontswitch\relax\mathcal}
\DeclareRobustCommand*\mit{\@fontswitch\relax\mathnormal}
%\newcommand\@pnumwidth{1.55em}
%\newcommand\@tocrmarg{2.55em}
%\newcommand\@dotsep{4.5}
%\setcounter{tocdepth}{3}
%\newcommand\tableofcontents{%
%    \section*{\contentsname
%        \@mkboth{%
%           \MakeUppercase\contentsname}{\MakeUppercase\contentsname}}%
%    \@starttoc{toc}%
%    }
%\newcommand*\l@part[2]{%
%  \ifnum \c@tocdepth >-2\relax
%    \addpenalty\@secpenalty
%    \addvspace{2.25em \@plus\p@}%
%    \begingroup
%      \parindent \z@ \rightskip \@pnumwidth
%      \parfillskip -\@pnumwidth
%      {\leavevmode
%       \large \bfseries #1\hfil \hb@xt@\@pnumwidth{\hss #2}}\par
%       \nobreak
%       \if@compatibility
%         \global\@nobreaktrue
%         \everypar{\global\@nobreakfalse\everypar{}}%
%      \fi
%    \endgroup
%  \fi}
%\newcommand*\l@section[2]{%
%  \ifnum \c@tocdepth >\z@
%    \addpenalty\@secpenalty
%    \addvspace{1.0em \@plus\p@}%
%    \setlength\@tempdima{1.5em}%
%    \begingroup
%      \parindent \z@ \rightskip \@pnumwidth
%      \parfillskip -\@pnumwidth
%      \leavevmode \bfseries
%      \advance\leftskip\@tempdima
%      \hskip -\leftskip
%      #1\nobreak\hfil \nobreak\hb@xt@\@pnumwidth{\hss #2}\par
%    \endgroup
%  \fi}
%\newcommand*\l@subsection{\@dottedtocline{2}{1.5em}{2.3em}}
%\newcommand*\l@subsubsection{\@dottedtocline{3}{3.8em}{3.2em}}
%\newcommand*\l@paragraph{\@dottedtocline{4}{7.0em}{4.1em}}
%\newcommand*\l@subparagraph{\@dottedtocline{5}{10em}{5em}}
%\newcommand\listoffigures{%
%    \section*{\listfigurename
%      \@mkboth{\MakeUppercase\listfigurename}%
%              {\MakeUppercase\listfigurename}}%
%    \@starttoc{lof}%
%    }
%\newcommand*\l@figure{\@dottedtocline{1}{1.5em}{2.3em}}
%\newcommand\listoftables{%
%    \section*{\listtablename
%      \@mkboth{%
%          \MakeUppercase\listtablename}{\MakeUppercase\listtablename}}%
%    \@starttoc{lot}%
%    }
%\let\l@table\l@figure
%======================================
%ENVIRONMENTS
%======================================
%ENVIRONMENT: indented
\newenvironment{indented}{\begin{indented}}{\end{indented}}
\newenvironment{varindent}[1]{\begin{varindent}{#1}}{\end{varindent}}
%
\def\indented{\list{}{\itemsep=0\p@\labelsep=0\p@\itemindent=0\p@
   \labelwidth=0\p@\leftmargin=\mathindent\topsep=0\p@\partopsep=0\p@
   \parsep=0\p@\listparindent=15\p@}\footnotesize\rm}
\let\endindented=\endlist 
\def\varindent#1{\setlength{\varind}{#1}%
   \list{}{\itemsep=0\p@\labelsep=0\p@\itemindent=0\p@
   \labelwidth=0\p@\leftmargin=\varind\topsep=0\p@\partopsep=0\p@
   \parsep=0\p@\listparindent=15\p@}\footnotesize\rm}
\let\endvarindent=\endlist 
%ENVIRONMENT: abstract
\newenvironment{abstract}{%
      \vspace{16pt plus3pt minus3pt}
      \begin{indented}
      \item[]{\bfseries \abstractname.}\quad\rm\ignorespaces} 
      {\end{indented}\vspace{10mm}}
%ENVIRONMENT: description
\newenvironment{description}
               {\list{}{\labelwidth\z@ \itemindent-\leftmargin
                        \let\makelabel\descriptionlabel}}
               {\endlist}
\newcommand\descriptionlabel[1]{\hspace\labelsep
                                \normalfont\bfseries #1}
%ENVIRONMENT: quotation
\newenvironment{quotation}
               {\list{}{\listparindent 1.5em%
                        \itemindent    \listparindent
                        \rightmargin   \leftmargin
                        \parsep        \z@ \@plus\p@}%
                \item[]}
               {\endlist}
%ENVIRONMENT: quote
\newenvironment{quote}
               {\list{}{\rightmargin\leftmargin}%
                \item[]}
               {\endlist}
%ENVIRONMENT: verse
\newenvironment{verse}
               {\let\\=\@centercr
                \list{}{\itemsep      \z@
                        \itemindent   -1.5em%
                        \listparindent\itemindent
                        \rightmargin  \leftmargin
                        \advance\leftmargin 1.5em}%
                \item[]}
               {\endlist}
%ENVIRONMENT: bibliography
\newdimen\bibindent
\setlength\bibindent{1.5em}
\def\thebibliography#1{\list
 {\hfil[\arabic{enumi}]}{\topsep=0\p@\parsep=0\p@
 \partopsep=0\p@\itemsep=0\p@
 \labelsep=5\p@\itemindent=-10\p@
 \settowidth\labelwidth{\footnotesize[#1]}%
 \leftmargin\labelwidth
 \advance\leftmargin\labelsep
 \advance\leftmargin -\itemindent
 \usecounter{enumi}}\footnotesize
 \def\newblock{\ }
 \sloppy\clubpenalty4000\widowpenalty4000
 \sfcode`\.=1000\relax}
\let\endthebibliography=\endlist
\def\numrefs#1{\begin{thebibliography}{#1}}
\def\endnumrefs{\end{thebibliography}}
\let\endbib=\endnumrefs
%%%%%%%%%%%%%%%%%%


%\newenvironment{thebibliography}[1]
%    {\section*{References}
%      \list{\@biblabel{\@arabic\c@enumiv}}%
%           {\settowidth\labelwidth{\@biblabel{#1}}%
%            \leftmargin\labelwidth
%           \advance\leftmargin\labelsep
%             \@openbib@code
%            \usecounter{enumiv}%
%            \let\p@enumiv\@empty
%            \renewcommand\theenumiv{\@arabic\c@enumiv}}%
%      \sloppy
%      \clubpenalty4000
%      \@clubpenalty \clubpenalty
%      \widowpenalty4000%
%      \sfcode`\.\@m}
%     {\def\@noitemerr
%       {\@latex@warning{Empty `thebibliography' environment}}%
%      \endlist}
%\newcommand\newblock{\hskip .11em\@plus.33em\@minus.07em}
%\let\@openbib@code\@empty
%ENVIRONMENT: theindex
\newenvironment{theindex}
               {\if@twocolumn
                  \@restonecolfalse
                \else
                  \@restonecoltrue
                \fi
                \columnseprule \z@
                \columnsep 35\p@
                \twocolumn[\section*{\indexname}]%
                \@mkboth{\MakeUppercase\indexname}%
                        {\MakeUppercase\indexname}%
                \thispagestyle{plain}\parindent\z@
                \parskip\z@ \@plus .3\p@\relax
                \let\item\@idxitem}
               {\if@restonecol\onecolumn\else\clearpage\fi}
\newcommand\@idxitem{\par\hangindent 40\p@}
\newcommand\subitem{\@idxitem \hspace*{20\p@}}
\newcommand\subsubitem{\@idxitem \hspace*{30\p@}}
\newcommand\indexspace{\par \vskip 10\p@ \@plus5\p@ \@minus3\p@\relax}
%=====================
\def\appendix{\@ifnextchar*{\@appendixstar}{\@appendix}}
\def\@appendix{\eqnobysec\@appendixstar}
\def\@appendixstar{\@@par
 \ifnumbysec                         %  Added 30/4/94 to get Table A1,
 \@addtoreset{table}{section}        %  Table B1 etc if numbering by
 \@addtoreset{figure}{section}\fi    %  section
 \setcounter{section}{0}
 \setcounter{subsection}{0}
 \setcounter{subsubsection}{0}
 \setcounter{equation}{0}
 \setcounter{figure}{0}
 \setcounter{table}{0}
 \def\thesection{Appendix \Alph{section}}   
 \def\theequation{\ifnumbysec
      \Alph{section}.\arabic{equation}\else
      \Alph{section}\arabic{equation}\fi}  % Comment A\arabic{equation} maybe
 \def\thetable{\ifnumbysec                 % better? 15/4/95
      \Alph{section}\arabic{table}\else
      A\arabic{table}\fi}
 \def\thefigure{\ifnumbysec
      \Alph{section}\arabic{figure}\else
      A\arabic{figure}\fi}}
\def\noappendix{\setcounter{figure}{0}
     \setcounter{table}{0}
     \def\thetable{\arabic{table}}
     \def\thefigure{\arabic{figure}}}
\setlength\arraycolsep{5\p@}
\setlength\tabcolsep{6\p@}
\setlength\arrayrulewidth{.4\p@}
\setlength\doublerulesep{2\p@}
\setlength\tabbingsep{\labelsep}
\skip\@mpfootins = \skip\footins
\setlength\fboxsep{3\p@}
\setlength\fboxrule{.4\p@}
\renewcommand\theequation{\arabic{equation}}
% NAME OF STRUCTURES
\newcommand\contentsname{Contents}
\newcommand\listfigurename{List of Figures}
\newcommand\listtablename{List of Tables}
\newcommand\refname{References}
\newcommand\indexname{Index}
\newcommand\figurename{Figure}
\newcommand\tablename{Table}
\newcommand\partname{Part}
\newcommand\appendixname{Appendix}
\newcommand\abstractname{Abstract}
%Miscellaneous commands
\newcommand{\BibTeX}{{\rm B\kern-.05em{\sc i\kern-.025em b}\kern-.08em
    T\kern-.1667em\lower.7ex\hbox{E}\kern-.125emX}}
\newcommand{\jpcsit}{{\bfseries\itshape\selectfont Journal of Physics: Conference Series}}
\newcommand{\jpcs}{{\itshape\selectfont Journal of Physics: Conference Series}}
\newcommand{\iopp}{IOP Publishing}
\newcommand{\cls}{{\upshape\selectfont\texttt{jpconf.cls}}}
\newcommand{\corg}{conference organizer}
\newcommand\today{\number\day\space\ifcase\month\or
  January\or February\or March\or April\or May\or June\or
  July\or August\or September\or October\or November\or December\fi
  \space\number\year}
    \setlength\columnsep{10\p@}
\setlength\columnseprule{0\p@}
\newcommand{\Tables}{\clearpage\section*{Tables and table captions}
\def\fps@table{hp}\noappendix}
\newcommand{\Figures}{\clearpage\section*{Figure captions}
    \def\fps@figure{hp}\noappendix}
%
\newcommand{\Figure}[1]{\begin{figure}
  \caption{#1}
  \end{figure}}
%
\newcommand{\Table}[1]{\begin{table}
  \caption{#1}
  \begin{indented}
  \lineup
  \item[]\begin{tabular}{@{}l*{15}{l}}}
\def\endTable{\end{tabular}\end{indented}\end{table}}
\let\endtab=\endTable
%
\newcommand{\fulltable}[1]{\begin{table}
  \caption{#1}
  \lineup
  \begin{tabular*}{\textwidth}{@{}l*{15}{@{\extracolsep{0pt plus 12pt}}l}}}
\def\endfulltable{\end{tabular*}\end{table}}
%BIBLIOGRAPHY and References
%\newcommand{\Bibliography}[1]{\section*{References}\par\numrefs{#1}}
%\newcommand{\References}{\section*{References}\par\refs}
%\def\thebibliography#1{\list
% {\hfil[\arabic{enumi}]}{\topsep=0\p@\parsep=0\p@
% \partopsep=0\p@\itemsep=0\p@
% \labelsep=5\p@\itemindent=-10\p@
% \settowidth\labelwidth{\footnotesize[#1]}%
% \leftmargin\labelwidth
% \advance\leftmargin\labelsep
% \advance\leftmargin -\itemindent
% \usecounter{enumi}}\footnotesize
% \def\newblock{\ }
% \sloppy\clubpenalty4000\widowpenalty4000
% \sfcode`\.=1000\relax}
%\let\endthebibliography=\endlist
%\def\numrefs#1{\begin{thebibliography}{#1}}
%\def\endnumrefs{\end{thebibliography}}
%\let\endbib=\endnumrefs

\def\thereferences{\list{}{\topsep=0\p@\parsep=0\p@
 \partopsep=0\p@\itemsep=0\p@\labelsep=0\p@\itemindent=-18\p@
\labelwidth=0\p@\leftmargin=18\p@
}\footnotesize\rm
\def\newblock{\ }
\sloppy\clubpenalty4000\widowpenalty4000
\sfcode`\.=1000\relax}%
\let\endthereferences=\endlist
% MISC EQUATRION STUFF
%\def\[{\relax\ifmmode\@badmath\else
% \begin{trivlist}
% \@beginparpenalty\predisplaypenalty
% \@endparpenalty\postdisplaypenalty
% \item[]\leavevmode
% \hbox to\linewidth\bgroup$ \displaystyle
% \hskip\mathindent\bgroup\fi}
%\def\]{\relax\ifmmode \egroup $\hfil \egroup \end{trivlist}\else \@badmath \fi}
%\def\equation{\@beginparpenalty\predisplaypenalty
% \@endparpenalty\postdisplaypenalty
%\refstepcounter{equation}\trivlist \item[]\leavevmode
% \hbox to\linewidth\bgroup $ \displaystyle
%\hskip\mathindent}
%\def\endequation{$\hfil \displaywidth\linewidth\@eqnnum\egroup \endtrivlist}
%\@namedef{equation*}{\[}
%\@namedef{endequation*}{\]}
%\def\eqnarray{\stepcounter{equation}\let\@currentlabel=\theequation
%\global\@eqnswtrue
%\global\@eqcnt\z@\tabskip\mathindent\let\\=\@eqncr
%\abovedisplayskip\topsep\ifvmode\advance\abovedisplayskip\partopsep\fi
%\belowdisplayskip\abovedisplayskip
%\belowdisplayshortskip\abovedisplayskip
%\abovedisplayshortskip\abovedisplayskip
%$$\halign to
%\linewidth\bgroup\@eqnsel$\displaystyle\tabskip\z@
% {##{}}$&\global\@eqcnt\@ne $\displaystyle{{}##{}}$\hfil
% &\global\@eqcnt\tw@ $\displaystyle{{}##}$\hfil
% \tabskip\@centering&\llap{##}\tabskip\z@\cr}
%\def\endeqnarray{\@@eqncr\egroup
% \global\advance\c@equation\m@ne$$\global\@ignoretrue }
%\mathindent = 6pc
%%
%\def\eqalign#1{\null\vcenter{\def\\{\cr}\openup\jot\m@th
%  \ialign{\strut$\displaystyle{##}$\hfil&$\displaystyle{{}##}$\hfil
%      \crcr#1\crcr}}\,}
%%
%\def\eqalignno#1{\displ@y \tabskip\z@skip
%  \halign to\displaywidth{\hspace{5pc}$\@lign\displaystyle{##}$%
%    \tabskip\z@skip
%    &$\@lign\displaystyle{{}##}$\hfill\tabskip\@centering
%    &\llap{$\@lign\hbox{\rm##}$}\tabskip\z@skip\crcr
%    #1\crcr}}
%%
\newif\ifnumbysec
\def\theequation{\ifnumbysec
      \arabic{section}.\arabic{equation}\else
      \arabic{equation}\fi}
\def\eqnobysec{\numbysectrue\@addtoreset{equation}{section}}
\newcounter{eqnval}
\def\numparts{\addtocounter{equation}{1}%
     \setcounter{eqnval}{\value{equation}}%
     \setcounter{equation}{0}%
     \def\theequation{\ifnumbysec
     \arabic{section}.\arabic{eqnval}{\it\alph{equation}}%
     \else\arabic{eqnval}{\it\alph{equation}}\fi}}
\def\endnumparts{\def\theequation{\ifnumbysec
     \arabic{section}.\arabic{equation}\else
     \arabic{equation}\fi}%
     \setcounter{equation}{\value{eqnval}}}
%
\def\cases#1{%
     \left\{\,\vcenter{\def\\{\cr}\normalbaselines\openup1\jot\m@th%
     \ialign{\strut$\displaystyle{##}\hfil$&\tqs
     \rm##\hfil\crcr#1\crcr}}\right.}%
\def\eqalign#1{\null\vcenter{\def\\{\cr}\openup\jot\m@th
  \ialign{\strut$\displaystyle{##}$\hfil&$\displaystyle{{}##}$\hfil
      \crcr#1\crcr}}\,}
% OTHER  USEFUL BITS
\newcommand{\e}{\mathrm{e}}
\newcommand{\rme}{\mathrm{e}}
\newcommand{\rmi}{\mathrm{i}}
\newcommand{\rmd}{\mathrm{d}}
\renewcommand{\qquad}{\hspace*{25pt}}
\newcommand{\tdot}[1]{\stackrel{\dots}{#1}}   % Added 1/9/94
\newcommand{\tqs}{\hspace*{25pt}}
\newcommand{\fl}{\hspace*{-\mathindent}}
\newcommand{\Tr}{\mathop{\mathrm{Tr}}\nolimits}
\newcommand{\tr}{\mathop{\mathrm{tr}}\nolimits}
\newcommand{\Or}{\mathord{\mathrm{O}}} %changed from \mathop 20/1/95
\newcommand{\lshad}{[\![}
\newcommand{\rshad}{]\!]}
\newcommand{\case}[2]{{\textstyle\frac{#1}{#2}}}
\def\pt(#1){({\it #1\/})}
\newcommand{\dsty}{\displaystyle}
\newcommand{\tsty}{\textstyle}
\newcommand{\ssty}{\scriptstyle}
\newcommand{\sssty}{\scriptscriptstyle}
\def\lo#1{\llap{${}#1{}$}}
\def\eql{\llap{${}={}$}}
\def\lsim{\llap{${}\sim{}$}}
\def\lsimeq{\llap{${}\simeq{}$}}
\def\lequiv{\llap{${}\equiv{}$}}
%
\newcommand{\eref}[1]{(\ref{#1})}
%\newcommand{\eqref}[1]{Equation (\ref{#1})}
%\newcommand{\Eqref}[1]{Equation (\ref{#1})}
\newcommand{\sref}[1]{section~\ref{#1}}
\newcommand{\fref}[1]{figure~\ref{#1}}
\newcommand{\tref}[1]{table~\ref{#1}}
\newcommand{\Sref}[1]{Section~\ref{#1}}
\newcommand{\Fref}[1]{Figure~\ref{#1}}
\newcommand{\Tref}[1]{Table~\ref{#1}}
\newcommand{\opencircle}{\mbox{\Large$\circ\,$}}  % moved Large outside maths
\newcommand{\opensquare}{\mbox{$\rlap{$\sqcap$}\sqcup$}}
\newcommand{\opentriangle}{\mbox{$\triangle$}}
\newcommand{\opentriangledown}{\mbox{$\bigtriangledown$}}
\newcommand{\opendiamond}{\mbox{$\diamondsuit$}}
\newcommand{\fullcircle}{\mbox{{\Large$\bullet\,$}}} % moved Large outside maths
\newcommand{\fullsquare}{\,\vrule height5pt depth0pt width5pt}
\newcommand{\dotted}{\protect\mbox{${\mathinner{\cdotp\cdotp\cdotp\cdotp\cdotp\cdotp}}$}}
\newcommand{\dashed}{\protect\mbox{-\; -\; -\; -}}
\newcommand{\broken}{\protect\mbox{-- -- --}}
\newcommand{\longbroken}{\protect\mbox{--- --- ---}}
\newcommand{\chain}{\protect\mbox{--- $\cdot$ ---}}
\newcommand{\dashddot}{\protect\mbox{--- $\cdot$ $\cdot$ ---}}
\newcommand{\full}{\protect\mbox{------}}

\def\;{\protect\psemicolon}
\def\psemicolon{\relax\ifmmode\mskip\thickmuskip\else\kern .3333em\fi}
\def\lineup{\def\0{\hbox{\phantom{0}}}%
    \def\m{\hbox{$\phantom{-}$}}%
    \def\-{\llap{$-$}}}
%
%%%%%%%%%%%%%%%%%%%%%
% Tables rules      %
%%%%%%%%%%%%%%%%%%%%%

\newcommand{\boldarrayrulewidth}{1\p@} 
% Width of bold rule in tabular environment.

\def\bhline{\noalign{\ifnum0=`}\fi\hrule \@height  
\boldarrayrulewidth \futurelet \@tempa\@xhline}

\def\@xhline{\ifx\@tempa\hline\vskip \doublerulesep\fi
      \ifnum0=`{\fi}}

%
% Rules for tables with extra space around
%
\newcommand{\br}{\ms\bhline\ms}
\newcommand{\mr}{\ms\hline\ms}
%
\newcommand{\centre}[2]{\multispan{#1}{\hfill #2\hfill}}
\newcommand{\crule}[1]{\multispan{#1}{\hspace*{\tabcolsep}\hrulefill
  \hspace*{\tabcolsep}}}
\newcommand{\fcrule}[1]{\ifnum\thetabtype=1\multispan{#1}{\hrulefill
  \hspace*{\tabcolsep}}\else\multispan{#1}{\hrulefill}\fi}
%
% Extra spaces for tables and displayed equations
%
\newcommand{\ms}{\noalign{\vspace{3\p@ plus2\p@ minus1\p@}}}
\newcommand{\bs}{\noalign{\vspace{6\p@ plus2\p@ minus2\p@}}}
\newcommand{\ns}{\noalign{\vspace{-3\p@ plus-1\p@ minus-1\p@}}}
\newcommand{\es}{\noalign{\vspace{6\p@ plus2\p@ minus2\p@}}\displaystyle}%
%
\newcommand{\etal}{{\it et al\/}\ }
\newcommand{\dash}{------}
\newcommand{\nonum}{\par\item[]}                %\par added 1/9/93
\newcommand{\mat}[1]{\underline{\underline{#1}}}
%
% abbreviations for IOPP journals
%
\newcommand{\CQG}{{\it Class. Quantum Grav.} }
\newcommand{\CTM}{{\it Combust. Theory Modelling\/} }
\newcommand{\DSE}{{\it Distrib. Syst. Engng\/} }
\newcommand{\EJP}{{\it Eur. J. Phys.} } 
\newcommand{\HPP}{{\it High Perform. Polym.} }              % added 4/5/93
\newcommand{\IP}{{\it Inverse Problems\/} }
\newcommand{\JHM}{{\it J. Hard Mater.} }                    % added 4/5/93
\newcommand{\JO}{{\it J. Opt.} }
\newcommand{\JOA}{{\it J. Opt. A: Pure Appl. Opt.} }
\newcommand{\JOB}{{\it J. Opt. B: Quantum Semiclass. Opt.} }
\newcommand{\JPA}{{\it J. Phys. A: Math. Gen.} }
\newcommand{\JPB}{{\it J. Phys. B: At. Mol. Phys.} }      %1968-87
\newcommand{\jpb}{{\it J. Phys. B: At. Mol. Opt. Phys.} } %1988 and onwards
\newcommand{\JPC}{{\it J. Phys. C: Solid State Phys.} }   %1968--1988
\newcommand{\JPCM}{{\it J. Phys.: Condens. Matter\/} }    %1989 and onwards
\newcommand{\JPD}{{\it J. Phys. D: Appl. Phys.} }
\newcommand{\JPE}{{\it J. Phys. E: Sci. Instrum.} }
\newcommand{\JPF}{{\it J. Phys. F: Met. Phys.} }
\newcommand{\JPG}{{\it J. Phys. G: Nucl. Phys.} }         %1975--1988
\newcommand{\jpg}{{\it J. Phys. G: Nucl. Part. Phys.} }   %1989 and onwards
\newcommand{\MSMSE}{{\it Modelling Simulation Mater. Sci. Eng.} }
\newcommand{\MST}{{\it Meas. Sci. Technol.} }                 %1990 and onwards
\newcommand{\NET}{{\it Network: Comput. Neural Syst.} }
\newcommand{\NJP}{{\it New J. Phys.} }
\newcommand{\NL}{{\it Nonlinearity\/} }
\newcommand{\NT}{{\it Nanotechnology} }
\newcommand{\PAO}{{\it Pure Appl. Optics\/} }
\newcommand{\PM}{{\it Physiol. Meas.} }                        % added 4/5/93
\newcommand{\PMB}{{\it Phys. Med. Biol.} }
\newcommand{\PPCF}{{\it Plasma Phys. Control. Fusion\/} }      % added 4/5/93
\newcommand{\PSST}{{\it Plasma Sources Sci. Technol.} }
\newcommand{\PUS}{{\it Public Understand. Sci.} }
\newcommand{\QO}{{\it Quantum Opt.} }
\newcommand{\QSO}{{\em Quantum Semiclass. Opt.} }
\newcommand{\RPP}{{\it Rep. Prog. Phys.} }
\newcommand{\SLC}{{\it Sov. Lightwave Commun.} }               % added 4/5/93 
\newcommand{\SST}{{\it Semicond. Sci. Technol.} }
\newcommand{\SUST}{{\it Supercond. Sci. Technol.} }
\newcommand{\WRM}{{\it Waves Random Media\/} }
\newcommand{\JMM}{{\it J. Micromech. Microeng.\/} }
%
% Other commonly quoted journals
%
\newcommand{\AC}{{\it Acta Crystallogr.} }
\newcommand{\AM}{{\it Acta Metall.} }
\newcommand{\AP}{{\it Ann. Phys., Lpz.} }
\newcommand{\APNY}{{\it Ann. Phys., NY\/} }
\newcommand{\APP}{{\it Ann. Phys., Paris\/} }
\newcommand{\CJP}{{\it Can. J. Phys.} }
\newcommand{\JAP}{{\it J. Appl. Phys.} }
\newcommand{\JCP}{{\it J. Chem. Phys.} }
\newcommand{\JJAP}{{\it Japan. J. Appl. Phys.} }
\newcommand{\JP}{{\it J. Physique\/} }
\newcommand{\JPhCh}{{\it J. Phys. Chem.} }
\newcommand{\JMMM}{{\it J. Magn. Magn. Mater.} }
\newcommand{\JMP}{{\it J. Math. Phys.} }
\newcommand{\JOSA}{{\it J. Opt. Soc. Am.} }
\newcommand{\JPSJ}{{\it J. Phys. Soc. Japan\/} }
\newcommand{\JQSRT}{{\it J. Quant. Spectrosc. Radiat. Transfer\/} }
\newcommand{\NC}{{\it Nuovo Cimento\/} }
\newcommand{\NIM}{{\it Nucl. Instrum. Methods\/} }
\newcommand{\NP}{{\it Nucl. Phys.} }
\newcommand{\PL}{{\it Phys. Lett.} }
\newcommand{\PR}{{\it Phys. Rev.} }
\newcommand{\PRL}{{\it Phys. Rev. Lett.} }
\newcommand{\PRS}{{\it Proc. R. Soc.} }
\newcommand{\PS}{{\it Phys. Scr.} }
\newcommand{\PSS}{{\it Phys. Status Solidi\/} }
\newcommand{\PTRS}{{\it Phil. Trans. R. Soc.} }
\newcommand{\RMP}{{\it Rev. Mod. Phys.} }
\newcommand{\RSI}{{\it Rev. Sci. Instrum.} }
\newcommand{\SSC}{{\it Solid State Commun.} }
\newcommand{\ZP}{{\it Z. Phys.} }
%===================
\pagestyle{headings}
\pagenumbering{arabic}
\raggedbottom
\onecolumn
\endinput
%%
%% End of file `jconf.cls'.
