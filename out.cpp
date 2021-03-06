#include "rx63.hpp"

//#define SHOWHEXCODES

static const char * const ccode[] =
{
	"eq", "ne", "c", "nc", "gtu", "leu", "pz", "n", "ge", "lt", "gt", "le", "o", "no", "bra", ""
};

static const char * const cmemex[] =
{
	".b", ".w", ".l", ".uw", ".ub", ".s", ".a"
};

static const char * const cregs[] =
{
	"psw", "pc", "usp", "fpsw", "", "", "", "", "bpsw", "bpc", "isp", "fintv", "intb", "", "", ""
};


inline void out_reg(int rgnum)
{
	out_register(ph.regNames[rgnum]);
}

void idaapi header(void)
{
	gen_header(GH_PRINT_PROC_ASM_AND_BYTESEX);
}

void idaapi footer(void)
{
	char buf[MAXSTR];

	MakeNull();

	tag_addstr(buf, buf+sizeof(buf), COLOR_ASMDIR, ash.end);
	MakeLine(buf, inf.indent);

	gen_cmt_line("end of file");
}

void idaapi segstart(ea_t ea)
{
	segment_t *sarea = getseg(ea);

	char sname[MAXNAMELEN];
	get_segm_name(sarea, sname, sizeof(sname));

	gen_cmt_line(COLSTR("segment %s", SCOLOR_AUTOCMT), sname);

	ea_t org = ea - get_segm_base(sarea);
	if (org != 0)
	{
		char buf[MAX_NUMBUF];
		btoa(buf, sizeof(buf), org);
		gen_cmt_line("%s %s", ash.origin, buf);
	}
}

void idaapi segend(ea_t ea)
{
	char sname[MAXNAMELEN];
	get_segm_name(getseg(ea-1), sname, sizeof(sname));
	gen_cmt_line("end of '%s'", sname);
}

inline ea_t real_address( ea_t ea )
{
	return ( ea >> 2 << 2 ) + 3 - ( ea & 3 );
}

bool is_need_add_empty_line()
{
	const int empty_line_ops[] = { RX63_smovf, RX63_jsr, RX63_b, RX63_bsr  };

	if ( get_first_cref_from (cmd.ea + cmd.size ) != BADADDR)
		return false;

	for(int i=0; i< qnumber( empty_line_ops ); i++)
	{
		if( empty_line_ops[i] == cmd.itype )
		{
			return true;
		}
	}
	return false;
}

void idaapi out(void)
{
	char buf[MAXSTR];
	init_output_buffer(buf, sizeof(buf));

#ifdef SHOWHEXCODES
	char bin[20];
	bin[0] = '\0';
	for(int i=0; i<cmd.size; i++)
	{
		char temp[10];
		qsnprintf(temp, sizeof(temp), "%02X", get_full_byte( real_address( cmd.ea + i)));
		qstrncat(bin, temp, sizeof(bin));
	}
	qstrncat(bin, " ", sizeof(bin));
	OutLine(bin);
#endif

	char cond_postfix[6];
	cond_postfix[0] = '\0';

	if ((cmd.auxpref & 0xf) != condition_t::none)
	{
		qstrncat(cond_postfix, ccode[cmd.auxpref & 0xf], sizeof(cond_postfix));
	}

	if ((cmd.auxpref >> 4) != 0)
	{
		qstrncat(cond_postfix, cmemex[(cmd.auxpref >> 4) - 1], sizeof(cond_postfix));
	}

	OutMnem(8, cond_postfix);

	out_one_operand(0);

	if ( cmd.Op2.type != o_void )
	{
		out_symbol(',');
		OutChar(' ');
		out_one_operand(1);
	}

	if ( cmd.Op3.type != o_void )
	{
		out_symbol(',');
		OutChar(' ');
		out_one_operand(2);
	}

	term_output_buffer();
	gl_comm = 1;
	MakeLine(buf);

	//if ( is_need_add_empty_line() )
	//	MakeLine("");
}

bool idaapi outop(op_t &x)
{
	switch ( x.type )
	{
		// register
		case o_reg:
			out_reg(x.reg);
			break;

		// immediate
		case o_imm:
			out_symbol('#');
			OutValue(x, OOFW_IMM);
			break;

		// displ dsp[reg]
		case o_displ:

			if (x.ld == ld_t::reg)
			{
				out_reg(x.reg);
			}
			else {
				if (x.value != 0)
					OutValue(x, OOFS_NOSIGN);
				out_symbol('[');
				out_reg(x.reg);
				out_symbol(']');

				// show memex?
				if ((x.memex & 0x10) == 0)
				{
					out_symbol('.');
					switch(x.memex)
					{
						case memex_t::l:
							out_symbol('l');
							break;
						case memex_t::uw:
							out_symbol('u');
							out_symbol('w');
							break;
						case memex_t::b:
							out_symbol('b');
							break;
						case memex_t::w:
							out_symbol('w');
							break;
						case memex_t::ub:
							out_symbol('u');
							out_symbol('b');
							break;
					}
				}
			}
			break;

		// address
		case o_near:
			if ( !out_name_expr(x, toEA(cmd.cs, x.addr), x.addr) )
				OutValue(x, OOF_ADDR | OOF_NUMBER | OOFS_NOSIGN | OOFW_32);
			break;

		case o_flag:
			switch(x.value & 0xf)
			{
				case cflag_t::flag_c:
					out_symbol('c');
					break;
				case cflag_t::flag_z:
					out_symbol('z');
					break;
				case cflag_t::flag_s:
					out_symbol('s');
					break;
				case cflag_t::flag_o:
					out_symbol('o');
					break;
				case cflag_t::flag_i:
					out_symbol('i');
					break;
				case cflag_t::flag_u:
					out_symbol('u');
					break;
			}
			break;
		case o_creg:
			out_register(cregs[x.value & 0xf]);
			break;

		case o_phrase:
			switch(x.phrase_type)
			{
				case rx63_phrases::f_r_r:
					out_symbol('[');
					out_reg(x.value&0xf);
					out_symbol(',');
					out_symbol(' ');
					out_reg(x.reg);
					out_symbol(']');
					break;
				case rx63_phrases::f_r_plus:
					out_symbol('[');
					out_reg(x.reg);
					out_symbol('+');
					out_symbol(']');
					break;
				case rx63_phrases::f_r_minus:
					out_symbol('[');
					out_symbol('-');
					out_reg(x.reg);
					out_symbol(']');
					break;
				case rx63_phrases::f_r_2_r:
					out_reg(x.value&0xf);
					out_symbol('-');
					out_reg(x.reg);
					break;
			}
			break;

	}
	return 1;
}

